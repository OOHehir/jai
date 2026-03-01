% jai(1)
% David Mazieres
% 

# NAME

jai - Jail an AI agent

# SYNOPSIS

`jai` [`-d` *dir*] [`-D`] *cmd* [*arg* ...] \
`jai` \
`jai` `-u`

# DESCRIPTION

`jai` is a super lightweight sandbox for AI agents requiring almost no
configuration.  It provides only casual security, so is not a
substitute for using a proper container to confine agents.  However it
is a great alternative to using no protection when you are thinking of
giving an agent full control of your account and all its files.
Compared to the later, `jai` can reduce the blast radius should things
go wrong.

Before using `jai`, if your home directory is on NFS, make
`$HOME/.jai` a symbolic link to a directory you own on a local file
system supporting extended attributes.

To get started, run `jai` with no arguments.  (If it is not setuid
root, you will need to run `sudo jai`.)  This will create an overlay
mount of your home directory in `/run/jai/$USER/sandboxed-home`.
Change to that directory and delete any sensitive files you don't want
your agent to have access to.  (Start with deleting a file you don't
care about, and verify that it only disappears from the sandbox, not
from your real home directory.)

Once you are satisfied with the sandbox, go to a project directory you
own and run `jai $SHELL`.  That will let you explore the sandboxed
environment.  You have complete access to the directory in which you
ran `jai`, but the rest of your home directory is sandboxed (changes
will not affect your real home directory), and the rest of the file
system outside your home directory is read-only.  If that works, exit
your shell and run `jai` _code-assistant_ for your favorite code
assistant.

# EXAMPLES

    jai claude

    jai codex

    jai opencode

# OPTIONS

`-d` *dir*
: Grant full access to directory *dir* and everything below in the
  jail.  You must own the directory.  This option implies `-D`--if you
  want to grant access to multiple directories and the current working
  directory, supply the current working directory as one of the `-d`
  options.

`-D`
: By default if you don't use `-d`, `jai` will grant access to the
  current working directory and anything below.  `-D` suppresses this
  behavior so that no directories are granted at all.

`-u`
: Removes the sandboxed home directory from `/run/jai`.

# ENvIRONMENT

`SUDO_USER`
: If run with real UID 0 and this environment variable exists, it will
  be taken as the user whose home directory should be sandboxed.  This
  makes it convenient to run `jai` via `sudo` if you don't want to
  install it setuid root.

# FILES

`$HOME/.jai`
: `jai` uses this to construct an overlay mount so that sandboxed code
  can believe it is writing to your home directory without actually
  doing so.  This directory requires extended attributes, so **must
  not be on NFS**.  Make it a symbolic link to a local directory owned
  by you if your home directory is on NFS.

`$HOME/.jai/changes`
: This "upper" directory is overlaid on your home directory and
  contains changes that have been made inside a jail.  If you make
  changes in this directory, you may need to tear down and recreate
  the sandboxed home directory with `jai -u`.

`/run/jai/$USER/sandboxed-home`
: Sandboxed home directory for jails.  You should delete any files
  with sensitive data in this directory so they will not be available
  in the jail.

`/run/jai/$USER/tmp`
: Private `/tmp` and `/var/tmp` directory made available in the jail.

