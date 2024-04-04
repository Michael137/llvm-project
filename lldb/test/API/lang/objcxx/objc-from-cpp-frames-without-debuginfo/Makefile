CXX_SOURCES := main.cpp
CXXFLAGS_EXTRAS := -g0

include Makefile.rules

#all: a.out stripped
#
#include Makefile.rules
#
#ifeq "$(MAKE_DSYM)" "YES"
#stripped: a.out.dSYM
#endif
#
#stripped: a.out
#	mkdir stripped
#	strip -Sx a.out -o stripped/a.out
#
#ifneq "$(CODESIGN)" ""
#	$(CODESIGN) -fs - stripped/a.out
#endif
#
#ifeq "$(MAKE_DSYM)" "YES"
#	cp -r a.out.dSYM stripped/a.out.dSYM
