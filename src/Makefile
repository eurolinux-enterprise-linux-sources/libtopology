VERSION := 0.3

# utility functions

# $(call source-to-object, source-file-list)
source-to-object = $(subst .c,.o,$(filter %.c,$1))

# $(subdirectory)
subdirectory = $(patsubst %/module.mk,%,			\
		 $(word						\
		   $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))

# $(call shared-lib-template, libfoo.so, sources, major, minor)
define shared-lib-template
  sources += $2
  major := $3
  minor := $4
  soname := $$(notdir $1).$$(major)
  target := $1
  targetm := $$(target).$$(major)
  targetmm := $$(targetm).$$(minor)
  inst_libs += $$(targetmm) $$(targetm) $$(target)
  libraries-shared += $$(targetmm) $$(targetm) $$(target)
  $$(targetmm): $(call source-to-object,$2)
	$(CC) $(CFLAGS) -shared -Wl,-soname,$$(soname) -o $$@ $$^
  $$(targetm): $$(targetmm)
	$(LN) $$(notdir $$<) $$@
  $$(target): $$(targetm)
	$(LN) $$(notdir $$<) $$@
endef

# fixme: need dependencies generated for tests
# $(call test-template, test-name.c)
define test-template
  $(subst .c,,$1): LDFLAGS += -L$(test_inst_prefix)/lib -ltopology
  $(subst .c,,$1): $1
  tests += $(subst .c,,$1)
endef

# $(call make-program, program-name, source-file-list)
define make-program
  programs += $1
  sources += $2
  # a bit hackier than I would like, but make doesn't support -lfoo
  # prerequisites that are also targets in the same makefile (-ltopology)
  $1: $2 lib/libtopology.so
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $$@ $$(filter %.c,$$^) -Llib -ltopology
endef

# Constants
CFLAGS := -Wall -Werror -g -fPIC -O
CPPFLAGS += -I include

# Commands
AWK := gawk
DOXYGEN := doxygen
INSTALL := install
RM := rm -rf
MKDIR := mkdir -p
GCC := gcc
CC := $(GCC)
CC_MAKEDEP := $(GCC)
ARFLAGS := r
LN := ln -s
CP := cp -a

modules   := lib programs tests
programs  :=
tests     =
sources   :=
libraries-shared :=
generated-docs := doc/generated

objects = $(call source-to-object,$(sources))
dependencies = $(subst .o,.d,$(objects))

cleanfiles = $(objects) $(programs) $(libraries-shared) $(dependencies) $(tests) $(test_inst_prefix) $(generated-docs)

# Installation-related variables
# These are recursive because the test code modifies prefix
prefix = /usr/local
inst_prefix = $(prefix)
inst_libdir = $(inst_prefix)/lib
inst_includedir = $(inst_prefix)/include

# fixme: these should be updated by submodules...
inst_libs :=
inst_headers := include/topology.h

# allow user to run valgrind on tests with VALGRIND=1 on cmdline
ifdef VALGRIND
  ifeq ("$(origin VALGRIND)", "command line")
    valgrind = valgrind -q --leak-check=full --track-fds=yes
  endif
endif

# our default target
all:

include $(addsuffix /module.mk,$(modules))

.PHONY: all
all: $(programs) $(libraries-shared)

.PHONY: test_install
test_install: prefix := $(test_inst_prefix)
test_install: install

.PHONY: test
# fixme: reliance on ordering of evaluation?
test: $(all) test_install $(tests)
	for t in $(tests) ; do \
	LIBTOPOLOGY_SYSFS_ROOT=$$t.sysfs \
		$(AWK) -f tests/sysfs-populate.awk \
		$$(dirname $$t)/sysfs/$$(basename $$t) && \
	LIBTOPOLOGY_SYSFS_ROOT=$$t.sysfs \
		LD_LIBRARY_PATH=$(test_inst_prefix)/lib $(valgrind) $$t ;\
	done

.PHONY: doc
doc:
	$(DOXYGEN) doc/Doxyfile

.PHONY: clean
clean:
	$(RM) $(cleanfiles)

.PHONY: install
install: $(inst_libs) $(inst_headers)
	@echo Installing to $(inst_prefix)
	@$(MKDIR) $(inst_libdir) && \
	$(CP) $(inst_libs) $(inst_libdir)
	@$(MKDIR) $(inst_includedir) && \
	$(INSTALL) -m 644 $(inst_headers) $(inst_includedir)

.PHONY: dist
dist: clean
	git archive --format=tar --prefix=libtopology-$(VERSION)/ HEAD \
	 | gzip > libtopology-$(VERSION).tar.gz

# automatic dependency generation
# from "Managing Projects With GNU Make" (Mecklenburg)
# 3rd ed. O'Reilly, 2005, p. 153

# dependencies := $(subst .c,.d,$(wildcard *.c))

ifneq "$(MAKECMDGOALS)" "clean"
  include $(dependencies)
endif

# $(call make-depend,source-file,object-file,depend-file)
define make-depend
	$(CC_MAKEDEP) 	\
		-M	\
		-MF $3	\
		-MP	\
		-MT $2	\
		$(CFLAGS)	\
		$(CPPFLAGS)	\
		$(TARGET_ARCH)	\
		$1
endef

%.d: %.c
	$(call make-depend,$<,$(subst .c,.o,$<),$@)
