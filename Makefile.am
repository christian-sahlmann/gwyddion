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
	mkinstalldirs \
	rules.msc \
	version.h

if MAINTAINER_MODE
dist-check-maintainer:
else
dist-check-maintainer:
	@echo "*** maintainer mode must be enabled in order to make dist"
	@false
endif

docs:
	(cd devel-docs && $(MAKE) $(AM_MAKEFLAGS) docs)

.PHONY: docs
