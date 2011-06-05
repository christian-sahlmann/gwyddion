# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion2.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule2.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess2.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw2.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets2.la

libgwyapp2includedir = $(pkgincludedir)/app
uidatadir = $(pkgdatadir)/ui


if MODULE_DEPENDENCIES
libgwyapp_libadd = \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)
endif

libgwyapp2include_HEADERS = \
	app.h \
	data-browser.h \
	datachooser.h \
	file.h \
	filelist.h \
	funcuse.h \
	glmaterial-editor.h \
	gradient-editor.h \
	gwyapp.h \
	gwyapptypes.h \
	gwymoduleutils.h \
	gwymoduleutils-file.h \
	gwyplaintool.h \
	gwytool.h \
	menu.h \
	resource-editor.h \
	settings.h \
	undo.h \
	validate.h \
	wait.h

noinst_HEADERS = \
	ige-mac-menu.h\
	mac_integration.h\
	authors.h \
	gwyappfilechooser.h \
	gwyappinternal.h \
	gwyddion.h

uidata_DATA = toolbox.xml

bin_PROGRAMS = gwyddion
lib_LTLIBRARIES = libgwyapp2.la

BUILT_SOURCES = \
	authors.h

if OS_WIN32
no_undefined = -no-undefined
export_symbols = -export-symbols libgwyapp2.def
libgwyapp2_la_DEPENDENCIES = libgwyapp2.def
endif

# Must init
EXTRA_DIST = \
	README.objdata \
	toolbox.xml
CLEANFILES = libgwyapp2.def
MAINTAINERCLEANFILES = authors.h

MKENUM_NAME = gwyapptypes
MKENUM_ID = GWY_APP_TYPES
MKENUM_HFILES = \
	$(srcdir)/data-browser.h \
	$(srcdir)/menu.h \
	$(srcdir)/gwymoduleutils-file.h \
	$(srcdir)/gwyplaintool.h \
	$(srcdir)/settings.h
include $(top_srcdir)/utils/mkenum.mk

if OS_DARWIN
carbon_cflags = -ObjC -framework AppKit -framework Carbon -framework ApplicationServices
endif

libgwyapp2.def: $(libgwyapp2_la_OBJECTS)
	$(AM_V_at)echo EXPORTS >libgwyapp2.def
	$(AM_V_GEN)for lo in $(libgwyapp2_la_OBJECTS); do \
		o=`sed "s/^pic_object='\\(.*\\)'/\\1/;t;d" "$$lo"`; \
		$(NM) --defined-only --demangle --no-sort "$$o" \
		| awk -F' ' '($$2=="T" && $$3 ~ /^gwy_/){print "     ",$$3}'; \
	done | sort >>libgwyapp2.def

AM_CFLAGS = $(carbon_cflags) @FFTW3_CFLAGS@ @UNIQUE_CFLAGS@ @COMMON_CFLAGS@
AM_CPPFLAGS = -I$(top_srcdir)

# In following order do:
# Any code change     C:   R++: A
# Any iface change    C++: 0:   A
# Adding ifaces       C:   R:   A++
# Changing ifaces     C:   R:   0
libversion = -version-info 7:2:7
#libversion = -release @LIBRARY_RELEASE@
libgwyapp2_la_LDFLAGS = @GTKGLEXT_LIBS@ @BASIC_LIBS@ -export-dynamic $(no_undefined) $(export_symbols) $(libversion)
libgwyapp2_la_CPPFLAGS = -DG_LOG_DOMAIN=\"GwyApp\" $(AM_CPPFLAGS)
libgwyapp2_la_LIBADD = $(libgwyapp_libadd)

libgwyapp2_la_SOURCES = \
	app.c \
	data-browser.c \
	datachooser.c \
	file.c \
	filelist.c \
	funcuse.c \
	glmaterial-editor.c \
	gradient-editor.c \
	gwyappfilechooser.c \
	gwyapptypes.c \
	gwymoduleutils.c \
	gwymoduleutils-file.c \
	gwyplaintool.c \
	gwytool.c \
	menu.c \
	settings.c \
	resource-editor.c \
	undo.c \
	validate.c \
	wait.c

gwyddion_SOURCES = \
	about.c \
	gwyddion.c \
	ige-mac-menu.c \
	mac_integration.c \
	meta.c \
	remote.c \
	remote-unique.c \
	remote-win32.c \
	remote-x11.c \
	splash.c \
	tips.c \
	toolbox.c


gwyddion_LDADD = @COMMON_LDFLAGS@ @GTKGLEXT_LIBS@ @FFTW3_LIBS@ @UNIQUE_LIBS@ @BASIC_LIBS@ $(LIBXMU) \
	libgwyapp2.la \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

# This is ugly a bit, but uses only very basic sed constructs
authors.h: ${top_srcdir}/AUTHORS
	$(AM_V_GEN){ \
		echo '/* This is a GENERATED file */'; \
		echo '/*< private_header >*/'; \
		echo 'static const gchar developers[] ='; \
		sed -n -e '/=== Developers ===/,/^$$/{' \
			-e 's/^[ =].*//' -e t -e 's/^$$//' -e t \
			-e 's/ *<.*//' -e 's/.*/    "&\\n"/' -e p -e '}' \
			${top_srcdir}/AUTHORS; \
		echo ';'; \
		echo 'static const gchar translators[] ='; \
		sed -n -e '/=== Translators ===/,/^$$/{' \
			-e 's/^[ =].*//' -e t -e 's/^$$//' -e t \
			-e 's/ *<.*//' -e 's/.*/    "&\\n"/' -e p -e '}' \
			${top_srcdir}/AUTHORS; \
		echo ';'; \
	} >authors.h

clean-local:
	rm -f core.* *~
