# @(#) $Id: Makefile.am 8698 2008-01-09 08:58:51Z xhorak $

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw2.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets2.la
libgwyapp = $(top_builddir)/app/libgwyapp2.la

EXTRA_DIST = gwyddion-thumbnailer.1

bin_PROGRAMS = gwyddion-thumbnailer
man1_MANS = gwyddion-thumbnailer.1

gwyddion_thumbnailer_SOURCES = gwyddion-thumbnailer.c

AM_CPPFLAGS = -I$(top_srcdir)
AM_CFLAGS = @COMMON_CFLAGS@

gwyddion_thumbnailer_LDADD = @GTKGLEXT_LIBS@ @BASIC_LIBS@ $(LIBTIFF) \
	$(libgwyapp) \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

CLEANFILES =

DISTCLEANFILES =

clean-local:
	rm -f core.* *~
