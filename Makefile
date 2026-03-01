
all: jai

CXX ?= c++
CXXFLAGS ?= -std=gnu++23 -Wall -Werror -ggdb
CPPFLAGS += $(shell pkg-config --cflags mount libacl)
LDLIBS += $(shell pkg-config --libs mount libacl)

OBJS = fs.o jai.o

all: jai jai.1

jai: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDLIBS)

.c.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $<

$(OBJS): jai.h

jai.1: jai.1.md
	-pandoc -s -w man jai.1.md -o jai.1
	@touch jai.1

clean:
	rm -f jai *~ *.o

.PHONY: all clean
