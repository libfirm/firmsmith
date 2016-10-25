# include user-defined makefile settings
-include config.mak

top_srcdir   ?= .
top_builddir ?= build
VPATH        ?= $(top_srcdir)/src

variant  ?= debug# Different libfirm variants (debug, optimize, profile, coverage)
srcdir   ?= $(top_srcdir)
builddir ?= $(top_builddir)/$(variant)

include config.default.mak

CC = llvm-gcc
AR ?= ar
DLLEXT ?= .so
LINK ?= $(CC)

CPPFLAGS += -I$(top_srcdir)/src -I$(builddir)
ifneq ("$(SYSTEM_INCLUDE_DIR)","")
	CPPFLAGS += -DSYSTEM_INCLUDE_DIR=\"$(SYSTEM_INCLUDE_DIR)\"
endif
ifneq ("$(LOCAL_INCLUDE_DIR)","")
	CPPFLAGS += -DLOCAL_INCLUDE_DIR=\"$(LOCAL_INCLUDE_DIR)\"
endif
ifneq ("$(COMPILER_INCLUDE_DIR)","")
	CPPFLAGS += -DCOMPILER_INCLUDE_DIR=\"$(COMPILER_INCLUDE_DIR)\"
endif
ifneq ("$(HOST_TRIPLE)","")
	CPPFLAGS += -DHOST_TRIPLE=\"$(HOST_TRIPLE)\"
endif
ifneq ("$(MULTILIB_M32_TRIPLE)","")
	CPPFLAGS += -DAPPEND_MULTILIB_DIRS -DMULTILIB_M32_TRIPLE=\"$(MULTILIB_M32_TRIPLE)\" -DMULTILIB_M64_TRIPLE=\"$(MULTILIB_M64_TRIPLE)\"
endif

CPPFLAGS := $(CPPFLAGS) $(FIRM_CPPFLAGS)

CFLAGS += -Wall -W -Wstrict-prototypes -Wmissing-prototypes -Wno-unused-function
# With -std=c99 we get __STRICT_ANSI__ which disables all posix declarations
# in cygwin, regardless of a set POSIX_C_SOURCE feature test macro.
ifneq ($(filter %cygwin %mingw32, $(shell $(CC) $(CFLAGS) -dumpmachine)),)
CFLAGS += -std=gnu99
else
CFLAGS += -std=c99 -pedantic
endif
CFLAGS_debug    = -O0 -g
CFLAGS_optimize = -O3 -fomit-frame-pointer -DNDEBUG -DNO_DEFAULT_VERIFY
CFLAGS_profile  = -pg -O3 -fno-inline
CFLAGS_coverage = --coverage -O0
CFLAGS += $(CFLAGS_$(variant))

LINKFLAGS_profile  = -pg
LINKFLAGS_coverage = --coverage
LINKFLAGS := $(LINKFLAGS) $(LINKFLAGS_$(variant)) $(FIRM_LIBS)

libfirmsmith_SOURCES := $(wildcard $(top_srcdir)/src/*/*.c)
libfirmsmith_OBJECTS = $(libfirmsmith_SOURCES:%.c=$(builddir)/%.o)
libfirmsmith_DEPS    = $(libfirmsmith_OBJECTS:%.o=%.d)
libfirmsmith_A       = $(builddir)/libfirmsmith.a
libfirmsmith_DLL     = $(builddir)/libfirmsmith$(DLLEXT)

firmsmith_SOURCES = main.c $(libfirmsmith_SOURCES)
firmsmith_OBJECTS = $(firmsmith_SOURCES:%.c=$(builddir)/%.o)
firmsmith_DEPS    = $(firmsmith_OBJECTS:%.o=%.d)
firmsmith_EXE     = $(builddir)/firmsmith

unittest_OBJECTS = $(libfirm_a) $(libfirmsmith_OBJECTS)

FIRMSMITHS = $(addsuffix .check, $(firmsmith_SOURCES) $(libfirmsmith_SOURCES))
FIRMSMITHOS = $(firmsmith_SOURCES:%.c=$(builddir)/cpb/%.o)
FIRMSMITHOS2 = $(firmsmith_SOURCES:%.c=$(builddir)/cpb2/%.o)

# This hides the noisy commandline outputs. Show them with "make V=1"
ifneq ($(V),1)
Q ?= @
endif

all: $(firmsmith_EXE)

# disable make builtin suffix rules
.SUFFIXES:

-include $(firmsmith_DEPS)

.PHONY: all bootstrap bootstrap2 clean check libfirm_subdir

DIRS   := $(sort $(dir $(firmsmith_OBJECTS) $(libfirmsmith_OBJECTS)))
UNUSED := $(shell mkdir -p $(DIRS) $(DIRS:$(builddir)/%=$(builddir)/cpb/%) $(DIRS:$(builddir)/%=$(builddir)/cpb2/%))

REVISIONH = $(builddir)/revision.h
REVISION ?= $(shell git --git-dir $(top_srcdir)/.git describe --abbrev=40 --always --dirty --match '')

# Flags to be used when firmsmith checks its own sourcecode for warnings
SELFCHECK_FLAGS ?= -Wall -Wno-shadow -Werror

# Update revision.h if necessary
UNUSED := $(shell \
	REV="\#define firmsmith_REVISION \"$(REVISION)\""; \
	echo "$$REV" | cmp -s - $(REVISIONH) 2> /dev/null || echo "$$REV" > $(REVISIONH) \
)
# determine if we can use "firmsmith-beta" as quickcheck
QUICKCHECK_DEFAULT := $(shell which firmsmith-beta 2> /dev/null || echo true) -fsyntax-only
QUICKCHECK ?= $(QUICKCHECK_DEFAULT)

$(firmsmith_EXE): $(LIBFIRM_FILE) $(firmsmith_OBJECTS)
	@echo 'LD $@'
	$(Q)$(LINK) $(firmsmith_OBJECTS) $(LIBFIRM_FILE) -o $@ $(LINKFLAGS)

$(libfirmsmith_A): $(libfirmsmith_OBJECTS)
	@echo 'AR $@'
	$(Q)$(AR) -crs $@ $^

$(libfirmsmith_DLL): $(libfirmsmith_OBJECTS) $(LIBFIRM_FILE_DLL)
	@echo 'LINK $@'
	$(Q)$(LINK) -shared $^ $(LINKFLAGS) -o "$@"

ifneq ("$(LIBFIRM_FILE)", "")
ifneq ("$(MAKECMDGOALS)", "clean")
$(LIBFIRM_FILE): libfirm_subdir
# Re-evaluate Makefile after libfirm_subdir has been executed
Makefile: libfirm_subdir
# Build libfirm in subdirectory
libfirm_subdir:
	$(Q)$(MAKE) -C $(FIRM_HOME) $(LIBFIRM_FILE_BASE)

$(LIBFIRM_FILE_DLL): libfirm_subdir_dll
libfirm_subdir_dll:
	$(Q)$(MAKE) -C $(FIRM_HOME) $(LIBFIRM_FILE_DLL_BASE)
endif
endif

check: $(FIRMSMITHS)
	@echo 'CHECK OPTIONS'
	$(Q)cd $(top_srcdir) ; support/check_options.py

bootstrap: firmsmith.bootstrap

bootstrap2: firmsmith.bootstrap2

%.c.check: %.c $(firmsmith_EXE)
	@echo 'CHECK $<'
	$(Q)$(firmsmith_EXE) $(CPPFLAGS) $(SELFCHECK_FLAGS) -fsyntax-only $<

$(builddir)/cpb/%.o: %.c $(firmsmith_EXE)
	@echo 'FIRMSMITH $<'
	$(Q)$(firmsmith_EXE) -m32 $(CPPFLAGS) -std=c99 -Wall -g3 -c $< -o $@

$(builddir)/cpb2/%.o: %.c firmsmith.bootstrap
	@echo 'FIRMSMITH.BOOTSTRAP $<'
	$(Q)./firmsmith.bootstrap -m32 $(CPPFLAGS) -Wall -g -c $< -o $@

firmsmith.bootstrap: $(FIRMSMITHOS)
	@echo 'LD $@'
	$(Q)$(firmsmith_EXE) -m32 $(FIRMSMITHOS) $(LIBFIRM_FILE) $(LINKFLAGS) -o $@

firmsmith.bootstrap2: firmsmith.bootstrap $(FIRMSMITHOS2)
	@echo 'LD $@'
	$(Q)./firmsmith.bootstrap -m32 $(FIRMSMITHOS2) $(LIBFIRM_FILE) $(LINKFLAGS) -o $@

$(builddir)/%.o: %.c
	@echo 'CC $@'
	$(Q)$(QUICKCHECK) $(CPPFLAGS) $(SELFCHECK_FLAGS) $<
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -MP -MMD -c -o $@ $<

clean:
	@echo 'CLEAN'
	$(Q)rm -rf $(builddir)

.PHONY: install
PREFIX ?= /usr/local
INSTALL ?= install
BINDIR = $(DESTDIR)$(PREFIX)/bin
MANDIR = $(DESTDIR)$(PREFIX)/share/man
install: $(firmsmith_EXE)
	$(INSTALL) -d $(DESTDIR)$(COMPILER_INCLUDE_DIR)
	$(INSTALL) -m0644 include/*.h $(DESTDIR)$(COMPILER_INCLUDE_DIR)
	$(INSTALL) -d $(BINDIR)
	$(INSTALL) -m0755 $< $(BINDIR)
	$(INSTALL) -d $(MANDIR)/man1
	$(INSTALL) -m0644 firmsmith.1 $(MANDIR)/man1

# Unit tests
UNITTESTS_SOURCES = $(subst $(srcdir)/unittests/,,$(wildcard $(srcdir)/unittests/*.c))
UNITTESTS         = $(UNITTESTS_SOURCES:%.c=$(builddir)/%.exe)
UNITTESTS_OK      = $(UNITTESTS_SOURCES:%.c=$(builddir)/%.ok)

$(builddir)/%.exe: $(srcdir)/unittests/%.c $(libfirmsmith_A) $(LIBFIRM_FILE)
	@echo LINK $<
	$(LINK) $(CFLAGS) $(CPPFLAGS) $(libfirm_CPPFLAGS) $+ -lm -o "$@"

$(builddir)/%.ok: $(builddir)/%.exe
	@echo EXEC $<
	$(Q)$< && touch "$@"

.PRECIOUS: $(UNITTESTS)
.PHONY: test
test: $(UNITTESTS_OK)
