# @(#) $Id$

pkgconfigdatadir = $(libdir)/pkgconfig

pkgconfigdata_DATA = gwyddion.pc

SUBDIRS = \
	libgwyddion \
	libprocess \
	libdraw \
	libgwydgets \
	libgwymodule \
	app \
	modules \
	plugins \
	pixmaps \
	devel-docs

M4TESTS = \
	m4/gtk-doc.m4 \
	m4/pkg.m4 \
	m4/tools.m4

EXTRA_DIST = \
	CODING-STANDARDS \
	autogen.sh \
	gwyddion.spec.in \
	gwyddion.spec\
	gwyddion.pc.in \
	$(M4TESTS)

docs:
	(cd devel-docs && $(MAKE) $(AM_MAKEFLAGS) docs)

.PHONY: docs
