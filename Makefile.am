# @(#) $Id$

pkgconfigdatadir = $(libdir)/pkgconfig
gwyddionappdir = $(datadir)/applications

pkgconfigdata_DATA =\
	gwyddion.pc

gwyddionapp_DATA = \
	gwyddion.desktop \
	x-gwyddion.desktop

SUBDIRS = \
	libgwyddion \
	libprocess \
	libdraw \
	libgwydgets \
	libgwymodule \
	app \
	modules \
	plugins \
	po \
	pixmaps \
	perl \
	python \
	devel-docs

M4TESTS = \
	m4/cc-options.m4 \
	m4/codeset.m4 \
	m4/gettext.m4 \
	m4/glibc21.m4 \
	m4/gtk-doc.m4 \
	m4/iconv.m4 \
	m4/isc-posix.m4 \
	m4/lcmessage.m4 \
	m4/pkg.m4 \
	m4/progtest.m4 \
	m4/tools.m4

EXTRA_DIST = \
	make.msc \
	rules.msc \
	makefile.msc \
	CODING-STANDARDS \
	autogen.sh \
	gwyddion.spec.in \
	gwyddion.spec\
	gwyddion.iss.in \
	gwyddion.iss \
	gwyddion-devel.iss.in \
	gwyddion-devel.iss \
	gwyddion.pc.in \
	version.h.in \
	version.h \
	utils/update-msvc.py \
	utils/vim-syn-gen.py \
	utils/gwyddion-cfg.py \
	utils/gwyddion.vim \
	$(gwyddionapp_DATA) \
	$(M4TESTS)

if MAINTAINER_MODE
dist-check-maintainer:
else
dist-check-maintainer:
	@echo "*** maintainer mode must be enabled in order to make dist"
	@false
endif

docs:
	(cd devel-docs && $(MAKE) $(AM_MAKEFLAGS) docs)

# FIXME: depdendencies?
utils/gwyddion.vim: devel-docs/*/*-decl.txt
	./utils/vim-syn-gen.py ./utils/gwyddion-cfg.py >$@

.PHONY: docs
