# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw2.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets2.la
libgwyapp = $(top_builddir)/app/libgwyapp2.la

kde4dir = $(KDE4_MODULE_DIR)

if OS_WIN32
no_undefined = -no-undefined
endif

# When the thumbnailer is not built, we still must distribute its source code.
if KDE4_THUMBNAILER
gwythumbcreator_sources =
gwythumbcreator_module = gwythumbcreator.la
else
gwythumbcreator_sources = gwythumbcreator.cc
gwythumbcreator_module =
endif

bin_PROGRAMS = gwyddion-thumbnailer
man1_MANS = gwyddion-thumbnailer.1
kde4_LTLIBRARIES = $(gwythumbcreator_module)

EXTRA_DIST = gwyddion-thumbnailer.1 $(gwythumbcreator_sources)

gwyddion_thumbnailer_SOURCES = gwyddion-thumbnailer.c
gwythumbcreator_la_SOURCES = gwythumbcreator.cc

AM_CPPFLAGS = -I$(top_srcdir)
AM_CFLAGS = @COMMON_CFLAGS@

gwyddion_thumbnailer_LDADD = @GTKGLEXT_LIBS@ @BASIC_LIBS@ $(LIBTIFF) \
	$(libgwyapp) \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

gwythumbcreator_la_CPPFLAGS = -I$(top_srcdir) @KDE4_CPPFLAGS@ -DGWYDDION_THUMBNAILER=\"$(bindir)/gwyddion-thumbnailer\"
gwythumbcreator_la_LDFLAGS = -avoid-version -module $(no_undefined)
gwythumbcreator_la_LIBADD = @KDE4_LDFLAGS@ @KDE4_LIBS@

CLEANFILES =

DISTCLEANFILES =

clean-local:
	rm -f core.* *~
