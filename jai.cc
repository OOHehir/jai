#include <cassert>
#include <cstring>
#include <filesystem>
#include <functional>
#include <utility>

#include <dirent.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

using std::filesystem::path;

path prog;

// Self-closing, movable file descriptor, use operator* to access value
struct Fd {
  int fd_{-1};

  Fd() noexcept = default;
  explicit Fd(int fd) : fd_(fd) {}
  Fd(Fd &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
  ~Fd() { reset(); }

  Fd &operator=(Fd &&other) noexcept
  {
    reset();
    fd_ = std::exchange(other.fd_, -1);
    return *this;
  }
  Fd &operator=(int fd) noexcept
  {
    reset();
    fd_ = fd;
    return *this;
  }

  int operator*() const noexcept { return fd_; }
  explicit operator bool() const { return fd_ >= 0; }
  void reset() noexcept
  {
    if (*this) {
      close(fd_);
      fd_ = -1;
    }
  }
};

// Generic RIAA destructor
struct Defer {
  std::function<void()> f_;

  Defer() noexcept = default;
  Defer(decltype(f_) f) noexcept : f_(std::move(f)) {}
  Defer(Defer &&other) noexcept : f_(other.release()) {}
  ~Defer() { reset(); }
  Defer &operator=(Defer &&other) noexcept
  {
    f_ = other.release();
    return *this;
  }
  decltype(f_) release() noexcept { return std::exchange(f_, nullptr); }
  void reset(decltype(f_) f = nullptr) noexcept
  {
    if (auto old = std::exchange(f_, f))
      old();
  }
};

struct Config {
  std::string user;
  uid_t uid = -1;
  gid_t gid = -1;
  path home;
  Fd homefd;
  Fd jaifd;

  void init();
  [[nodiscard]] Defer asuser();
  Fd mkudir(int dirfd, path p);
};

template<typename... Args>
[[noreturn]] void
syserr(std::format_string<Args...> fmt, Args &&...args)
{
  throw std::system_error(
      errno, std::system_category(),
      std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<typename E = std::runtime_error, typename... Args>
[[noreturn]] void
err(std::format_string<Args...> fmt, Args &&...args)
{
  throw E(std::vformat(fmt.get(), std::make_format_args(args...)));
}

template<std::integral I>
I
parsei(const char *s)
{
  const char *const e = s + strlen(s);
  I ret{};

  auto [p, ec] = std::from_chars(s, e, ret, 10);

  if (ec == std::errc::invalid_argument || p != e)
    err<std::invalid_argument>("{}: not an integer", s);
  if (ec == std::errc::result_out_of_range)
    err<std::out_of_range>("{}: overflow", s);

  return ret;
}

bool
dir_empty(int dirfd)
{
  int fd = dup(dirfd);
  if (fd < 0)
    syserr("dup");
  auto dir = fdopendir(fd);
  if (!dir) {
    close(fd);
    syserr("fdopendir");
  }
  Defer cleanup([dir] { closedir(dir); });

  while (auto de = readdir(dir))
    if (de->d_name[0] != '.' ||
        (de->d_name[1] != '\0' &&
         (de->d_name[1] != '.' || de->d_name[2] != '\0')))
      return false;
  return true;
}

void
Config::init()
{
  char buf[512];
  struct passwd pwbuf, *pw{};

  auto realuid = getuid();

  const char *envuser = getenv("SUDO_USER");
  if (realuid == 0 && envuser) {
    if (getpwnam_r(envuser, &pwbuf, buf, sizeof(buf), &pw))
      err("cannot find password entry for user {}", envuser);
  }
  else if (getpwuid_r(realuid, &pwbuf, buf, sizeof(buf), &pw))
    err("cannot find password entry for uid {}", uid);

  user = pw->pw_name;
  uid = pw->pw_uid;
  gid = pw->pw_gid;
  home = pw->pw_dir;

  // Paranoia about ptrace, because we will drop privileges to access
  // the file system as the user.
  prctl(PR_SET_DUMPABLE, 0);

  // Set all user permissions except user ID so we can easily drop
  // privileges in asuser.
  if (realuid == 0 && uid != 0) {
    if (initgroups(user.c_str(), gid))
      syserr("initgroups");
    if (setgid(gid))
      syserr("setgid");
  }

  auto cleanup = asuser();
  if (!(homefd = open(home.c_str(), O_PATH | O_CLOEXEC)))
    syserr("{}", home.string());
}

Defer
Config::asuser()
{
  if (!uid || geteuid())
    // If target is root or already dropped privileges, do nothing
    return {};
  if (seteuid(uid))
    syserr("seteuid");
  return Defer{[] { seteuid(0); }};
}

Fd
Config::mkudir(int dirfd, path p)
{
  auto restore = asuser();
  auto check = [this, &p](const struct stat &sb) {
    if (!S_ISDIR(sb.st_mode))
      err("{}: expected a directory", p.string());
    if (sb.st_uid != uid)
      err("{}: expected a directory owned by {}", p.string(), user);
    if ((sb.st_mode & 0700) != 0700)
      err("{}: expected a directory with owner rwx permissions", p.string());
  };
  struct stat sb;

  // Okay to follow symlink to existing directory owned by user
  if (Fd e{openat(dirfd, p.c_str(), O_RDONLY | O_CLOEXEC)}) {
    if (fstat(*e, &sb))
      syserr("fstat({})", p.string());
    check(sb);
    return e;
  }
  if (errno != ENOENT)
    syserr("open({})", p.string());

  if (mkdirat(dirfd, p.c_str(), 0755))
    syserr("mkdir({})", p.string());
  Defer cleanup([dirfd, &p] { unlinkat(dirfd, p.c_str(), AT_REMOVEDIR); });

  Fd d{openat(dirfd, p.c_str(),
              O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)};
  if (!d)
    syserr("open({})", p.string());

  // To void TOCTTOU bugs, make sure newly created directory is empty
  // and owned by user.
  if (!dir_empty(*d))
    err("mkudir: newly created directory {} not empty", p.string());
  if (fstat(*d, &sb))
    syserr("fstat({})", p.string());
  check(sb);

  cleanup.release();
  return d;
}

int
main(int argc, char **argv)
{
  if (argc > 0)
    prog = argv[0];
}
