# @(#) $Id$

pkgconfigdatadir = $(libdir)/pkgconfig
gwyddionappdir = $(datadir)/applications

pkgconfigdata_DATA =\
	gwyddion.pc

gwyddionapp_DATA = \
	gwyddion.desktop \
	x-gwyddion.desktop

SUBDIRS = \
	m4 \
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
	ruby \
	devel-docs

EXTRA_DIST = \
	CODING-STANDARDS \
	INSTALL.gwyddion \
	autogen.sh \
	gwyddion-devel.iss \
	gwyddion-devel.iss.in \
	gwyddion.iss \
	gwyddion.iss.in \
	gwyddion.pc.in \
	gwyddion.spec.in \
	gwyddion.spec\
	make.msc \
	makefile.msc \
	rules.msc \
	version.h \
	version.h.in \
	utils/update-msvc.py \
	utils/update-potfiles.sh \
	utils/vim-syn-gen.py \
	utils/gwyddion-cfg.py \
	utils/gwyddion.vim \
	$(gwyddionapp_DATA)

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
