# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la
libgwyapp = $(top_builddir)/app/libgwyapp.la

EXTRA_DIST = \
	update-msvc.py \
	update-potfiles.sh \
	vim-syn-gen.py

noinst_PROGRAMS = \
	dump-modules

dump_modules_SOURCES = \
	dump-modules.c

dump_modules_LDADD = @GTKGLEXT_LIBS@ \
	$(libgwyapp) \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

INCLUDES = -I$(top_srcdir)

clean-local:
	rm -f core.* *~

