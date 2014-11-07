# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw2.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets2.la
libgwyapp = $(top_builddir)/app/libgwyapp2.la
moduledir = $(top_srcdir)/modules
ugmapdatadir = $(pkgdatadir)

GUIDE_MAP = user-guide-modules

ugmapdata_DATA = $(GUIDE_MAP)

EXTRA_DIST = \
	extract-file-magic.py \
	genmarshal.mk \
	make-module-lists \
	mkenum.c.template \
	mkenum.h.template \
	mkenum.mk \
	modules-web.xsl \
	update-langs.py \
	update-potfiles.sh \
	$(GUIDE_MAP) \
	vim-syn-gen.py \
	gwyddion.nsi.template \
	generate-gwyddion-nsi.py \
	update-api-indices.py \
	bundler.in \
	mkosxlauncher.in

noinst_PROGRAMS = \
	dump-modules

noinst_SCRIPTS = \
	make-module-lists

dump_modules_SOURCES = \
	dump-modules.c

AM_CPPFLAGS = -I$(top_srcdir)
AM_CFLAGS = @COMMON_CFLAGS@

dump_modules_LDADD = @GTKGLEXT_LIBS@ @BASIC_LIBS@ \
	$(libgwyapp) \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

CLEANFILES = $(GUIDE_MAP).tmp

clean-local:
	rm -rf module-lists core.* *~
