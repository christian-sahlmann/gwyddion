# @(#) $Id$ */

if HAVE_TIFF
tiff_modules = jpkscan.la
endif


filedir = $(pkglibdir)/modules/file

file_LTLIBRARIES = \
	asciiexport.la \
	apefile.la \
	assing-afm.la \
	bcrfile.la \
	createc.la \
	ezdfile.la \
	gwyfile.la \
	hitachi-afm.la \
	nanoscope.la \
	nt-mdt.la \
	mifile.la \
	pixmap.la \
	rawfile.la \
	rhk-sm3.la \
	rhk-spm32.la \
	sis.la \
	spmlab.la \
	stmprg.la \
	stpfile.la \
	witfile.la \
	wsxmfile.la \
	$(tiff_modules)


noinst_HEADERS = \
	get.h \
	stmprg.h

EXTRA_DIST = \
	makefile.msc \
	makefile.msc.gwt


INCLUDES = -I$(top_srcdir)

AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\"


if OS_WIN32
no_undefined = -no-undefined
endif

AM_LDFLAGS = -avoid-version -module $(no_undefined)

if MODULE_DEPENDENCIES
MODULE_LIBTIFF = $(LIBTIFF)
module_libadd = \
	$(top_builddir)/app/libgwyapp.la \
	$(top_builddir)/libdraw/libgwydraw.la \
	$(top_builddir)/libgwyddion/libgwyddion.la \
	$(top_builddir)/libgwydgets/libgwydgets.la \
	$(top_builddir)/libgwymodule/libgwymodule.la \
	$(top_builddir)/libprocess/libgwyprocess.la \
	@GTK_LIBS@
else
MODULE_LIBTIFF =
endif


asciiexport_la_SOURCES = asciiexport.c
asciiexport_la_LIBADD = $(module_libadd)

apefile_la_SOURCES = apefile.c
apefile_la_LIBADD = $(module_libadd)

assing_afm_la_SOURCES = assing-afm.c
assing_afm_la_LIBADD = $(module_libadd)

bcrfile_la_SOURCES = bcrfile.c
bcrfile_la_LIBADD = $(module_libadd)

createc_la_SOURCES = createc.c
createc_la_LIBADD = $(module_libadd)

ezdfile_la_SOURCES = ezdfile.c
ezdfile_la_LIBADD = $(module_libadd)

gwyfile_la_SOURCES = gwyfile.c
gwyfile_la_LIBADD = $(module_libadd)

hitachi_afm_la_SOURCES = hitachi-afm.c
hitachi_afm_la_LIBADD = $(module_libadd)

if HAVE_TIFF
jpkscan_la_SOURCES = jpk.h jpkscan.c
jpkscan_la_LIBADD = $(module_libadd) $(MODULE_LIBTIFF)
endif

nanoscope_la_SOURCES = nanoscope.c
nanoscope_la_LIBADD = $(module_libadd)

nt_mdt_la_SOURCES = nt-mdt.c
nt_mdt_la_LIBADD = $(module_libadd)

mifile_la_SOURCES = mifile.c
mifile_la_LIBADD = $(module_libadd)

pixmap_la_SOURCES = pixmap.c
pixmap_la_LIBADD = $(module_libadd) $(MODULE_LIBTIFF)

rawfile_la_SOURCES = rawfile.c
rawfile_la_LIBADD = $(module_libadd)

rhk_sm3_la_SOURCES = rhk-sm3.c
rhk_sm3_la_LIBADD = $(module_libadd)

rhk_spm32_la_SOURCES = rhk-spm32.c
rhk_spm32_la_LIBADD = $(module_libadd)

sis_la_SOURCES = sis.c
sis_la_LIBADD = $(module_libadd)

spmlab_la_SOURCES = spmlab.c
spmlab_la_LIBADD = $(module_libadd)

stmprg_la_SOURCES = stmprg.c
stmprg_la_LIBADD = $(module_libadd)

stpfile_la_SOURCES = stpfile.c
stpfile_la_LIBADD = $(module_libadd)

witfile_la_SOURCES = witfile.c
witfile_la_LIBADD = $(module_libadd)

wsxmfile_la_SOURCES = wsxmfile.c
wsxmfile_la_LIBADD = $(module_libadd)


clean-local:
	rm -f core.* *~
