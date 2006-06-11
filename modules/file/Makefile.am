# @(#) $Id$ */

# When libTIFF is disabled, put the sources to EXTRA_DIST to include them in
# make dist.
if HAVE_TIFF
jpkscan_module = jpkscan.la
jpkscan_sources =
else
jpkscan_module =
jpkscan_sources = jpk.h jpkscan.c
endif

if HAVE_XML2
spml_module = spml.la
spml_sources =
else
spml_module =
spml_sources = spml.c
endif

filedir = $(pkglibdir)/modules/file

file_LTLIBRARIES = \
	assing-afm.la \
	apefile.la \
	asciiexport.la \
	bcrfile.la \
	burleigh.la \
	createc.la \
	ezdfile.la \
	ecsfile.la \
	gwyfile.la \
	hitachi-afm.la \
	$(jpkscan_module) \
	metropro.la \
	mifile.la \
	nanoscope.la \
	nanotop.la \
	nt-mdt.la \
	omicron.la \
	pixmap.la \
	pnifile.la \
	rawfile.la \
	rhk-sm3.la \
	rhk-spm32.la \
	sdfile.la \
	seiko.la \
	sis.la \
	$(spml_module) \
	spmlab.la \
	stmprg.la \
	stpfile.la \
	surffile.la \
	unisoku.la \
	witfile.la \
	wsxmfile.la

noinst_HEADERS = \
	err.h \
	get.h

EXTRA_DIST = \
	$(jpkscan_sources) \
	$(spml_sources) \
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

burleigh_la_SOURCES = burleigh.c
burleigh_la_LIBADD = $(module_libadd)

createc_la_SOURCES = createc.c
createc_la_LIBADD = $(module_libadd)

ecsfile_la_SOURCES = ecsfile.c
ecsfile_la_LIBADD = $(module_libadd)

ezdfile_la_SOURCES = ezdfile.c
ezdfile_la_LIBADD = $(module_libadd)

gwyfile_la_SOURCES = gwyfile.c
gwyfile_la_LIBADD = $(module_libadd)

#hdf4file_la_SOURCES = hdf4file.c
#hdf4file_la_LIBADD = $(module_libadd)

hitachi_afm_la_SOURCES = hitachi-afm.c
hitachi_afm_la_LIBADD = $(module_libadd)

if HAVE_TIFF
jpkscan_la_SOURCES = jpk.h jpkscan.c
jpkscan_la_LIBADD = $(module_libadd) $(MODULE_LIBTIFF)
endif

metropro_la_SOURCES = metropro.c
metropro_la_LIBADD = $(module_libadd)

mifile_la_SOURCES = mifile.c
mifile_la_LIBADD = $(module_libadd)

nanoscope_la_SOURCES = nanoscope.c
nanoscope_la_LIBADD = $(module_libadd)

nanotop_la_SOURCES = nanotop.c
nanotop_la_LIBADD = $(module_libadd)

nt_mdt_la_SOURCES = nt-mdt.c
nt_mdt_la_LIBADD = $(module_libadd)

omicron_la_SOURCES = omicron.c
omicron_la_LIBADD = $(module_libadd)

pixmap_la_SOURCES = pixmap.c
pixmap_la_LIBADD = $(module_libadd) $(MODULE_LIBTIFF)

pnifile_la_SOURCES = pnifile.c
pnifile_la_LIBADD = $(module_libadd)

rawfile_la_SOURCES = rawfile.c rawfilepreset.h
rawfile_la_LIBADD = $(module_libadd)

rhk_sm3_la_SOURCES = rhk-sm3.c
rhk_sm3_la_LIBADD = $(module_libadd)

rhk_spm32_la_SOURCES = rhk-spm32.c
rhk_spm32_la_LIBADD = $(module_libadd)

sdfile_la_SOURCES = sdfile.c
sdfile_la_LIBADD = $(module_libadd)

seiko_la_SOURCES = seiko.c
seiko_la_LIBADD = $(module_libadd)

sis_la_SOURCES = sis.c
sis_la_LIBADD = $(module_libadd)

if HAVE_XML2
spml_la_SOURCES = spml.c spml-utils.h
spml_la_CFLAGS = @XML2_CFLAGS@
spml_la_LIBADD = $(module_libadd) @XML2_LIBS@
endif

spmlab_la_SOURCES = spmlab.c
spmlab_la_LIBADD = $(module_libadd)

stmprg_la_SOURCES = stmprg.c stmprg.h
stmprg_la_LIBADD = $(module_libadd)

stpfile_la_SOURCES = stpfile.c
stpfile_la_LIBADD = $(module_libadd)

surffile_la_SOURCES = surffile.c
surffile_la_LIBADD = $(module_libadd)

unisoku_la_SOURCES = unisoku.c
unisoku_la_LIBADD = $(module_libadd)

witfile_la_SOURCES = witfile.c
witfile_la_LIBADD = $(module_libadd)

wsxmfile_la_SOURCES = wsxmfile.c
wsxmfile_la_LIBADD = $(module_libadd)

clean-local:
	rm -f core.* *~
