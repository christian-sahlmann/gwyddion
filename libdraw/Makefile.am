# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la

libgwydraw2includedir = $(pkgincludedir)/libdraw

if MODULE_DEPENDENCIES
libgwydraw_libadd = \
	$(libgwyprocess) \
	$(libgwyddion)
endif

libgwydraw2include_HEADERS = \
	gwydraw.h \
	gwydrawenums.h \
	gwyglmaterial.h \
	gwygradient.h \
	gwypixfield.h \
	gwyrgba.h \
	gwyselection.h

lib_LTLIBRARIES = libgwydraw2.la

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt \
	libgwydraw2.def

if OS_WIN32
no_undefined = -no-undefined
export_symbols = -export-symbols libgwydraw2.def
libgwydraw2_la_DEPENDENCIES = libgwydraw2.def
endif

noinst_HEADERS = \
	gwydrawinternal.h

libgwydraw2.def: $(libgwydraw2_la_OBJECTS)
	@echo EXPORTS >libgwydraw2.def
	@for lo in $(libgwydraw2_la_OBJECTS); do \
		o=`sed "s/^pic_object='\\(.*\\)'/\\1/;t;d" "$$lo"`; \
		$(NM) --defined-only --demangle --no-sort "$$o" \
		| awk -F' ' '($$2=="T" && $$3 ~ /^gwy_/){print "     ",$$3}'; \
	done | sort >>libgwydraw2.def

AM_CFLAGS = @COMMON_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir) -DG_LOG_DOMAIN=\"GwyDraw\"

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libversion = -version-info 1:0:1
#libversion = -release @LIBRARY_RELEASE@
libgwydraw2_la_LDFLAGS = @GTK_LIBS@ @BASIC_LIBS@ -export-dynamic $(no_undefined) $(export_symbols) $(libversion)
libgwydraw2_la_LIBADD = $(libgwydraw_libadd)

libgwydraw2_la_SOURCES = \
	gwydraw.c \
	gwyglmaterial.c \
	gwygradient.c \
	gwypixfield.c \
	gwyrgba.c \
	gwyselection.c

MAINTAINERCLEANFILES = \
	makefile.msc \
	libgwydraw2.def

clean-local:
	rm -f core.* *~

