# @(#) $Id$

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
	data \
	utils \
	devel-docs

EXTRA_DIST = \
	INSTALL.gwyddion \
	autogen.sh \
	gwyddion.spec\
	make.msc \
	makefile.msc \
	rules.msc \
	version.h

DISTCLEANFILES = gwyconfig.h

ACLOCAL_AMFLAGS = -I m4

if MAINTAINER_MODE
dist-check-maintainer:
else
dist-check-maintainer:
	@echo "*** maintainer mode must be enabled in order to make dist"
	@false
endif

docs:
	(cd devel-docs && $(MAKE) $(AM_MAKEFLAGS) docs)

configexecincludedir = $(pkglibdir)/include
configexecinclude_DATA = gwyconfig.h

.PHONY: docs
