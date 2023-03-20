CC ?= cc

PREFIX   := /usr
LIBDIR   := $(PREFIX)/lib
BUILDDIR := build

CFLAGS := -std=c17 -Werror -Wextra -O3 \
	  -march=native -fomit-frame-pointer -funroll-loops

LDFLAGS := -fpic -flto

CP := cp -f
ECHO := @echo

NAME := mleak
DESCRIPTION := Library for catching memory leaks, double-frees along with invalid pointers to free\(\) and realloc\(\).
VERSION := 0.1.1

SOURCE := src/mleak.c
HEADER := src/mleak.h
OBJECT := $(BUILDDIR)/mleak.o
STATIC := $(BUILDDIR)/libmleak.a
SHARED := $(BUILDDIR)/libmleak.so
PKGCFG := $(BUILDDIR)/libmleak.pc

all: $(SHARED) $(STATIC) $(PKGCFG)

$(OBJECT): $(BUILDDIR) $(SOURCE) $(HEADER)
	$(CC) $(LDFLAGS) $(CFLAGS) $(SOURCE) -c -o $@

$(SHARED): $(OBJECT)
	$(CC) $(LDFLAGS) $(OBJECT) -shared -o $@

$(STATIC): $(OBJECT)
	ar rcs $@ $(OBJECT)

$(PKGCFG):
	$(ECHO) prefix=$(PREFIX) > $(PKGCFG)
	$(ECHO) libdir=$$\{prefix\}/$(notdir $(LIBDIR)) >> $(PKGCFG)
	$(ECHO) Name: $(NAME) >> $(PKGCFG)
	$(ECHO) Description: $(DESCRIPTION) >> $(PKGCFG)
	$(ECHO) Version: $(VERSION) >> $(PKGCFG)
	$(ECHO) Libs: -L$$\{libdir\} -lmleak >> $(PKGCFG)

$(BUILDDIR):
	$(RM) $(BUILDDIR)
	mkdir -p $(BUILDDIR)

install: $(SHARED) $(STATIC) $(PKGCFG)
	$(CP) $(PKGCFG) $(LIBDIR)/pkgconfig
	$(CP) $(SHARED) $(LIBDIR)
	$(CP) $(STATIC) $(LIBDIR)

uninstall:
	$(RM) $(LIBDIR)/$(notdir $(SHARED))
	$(RM) $(LIBDIR)/$(notdir $(STATIC))
	$(RM) $(LIBDIR)/pkgconfig/$(notdir $(PKGCFG))
clean:
	$(RM) -r $(BUILDDIR)
	$(RM) compile_flags.txt

compile_flags.txt:
	$(ECHO) $(CFLAGS) | tr " " "\n" > compile_flags.txt

.PHONY: install uninstall clean compile_flags.txt
