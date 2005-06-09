# @(#) $Id$ */

filedir = $(pkglibdir)/modules/file

module_ldflags = -shared -Wl,-soname

file_PROGRAMS = \
	asciiexport.so \
	apefile.so \
	assing-afm.so \
	bcrfile.so \
	createc.so \
	ezdfile.so \
	gwyfile.so \
	jpkscan.so \
	nanoscope.so \
	nt-mdt.so \
	pixmap.so \
	rawfile.so \
	rhk-spm32.so \
	sis.so \
	spmlab.so \
	stmprg.so \
	stpfile.so \
	wsxmfile.so

noinst_HEADERS = \
	get.h \
	stmprg.h

EXTRA_DIST = \
	makefile.msc

asciiexport_so_SOURCES = asciiexport.c
asciiexport_so_LDFLAGS = $(module_ldflags),asciiexport.so

apefile_so_SOURCES = apefile.c
apefile_so_LDFLAGS = $(module_ldflags),apefile.so

assing_afm_so_SOURCES = assing-afm.c
assing_afm_so_LDFLAGS = $(module_ldflags),assing-afm.so

bcrfile_so_SOURCES = bcrfile.c
bcrfile_so_LDFLAGS = $(module_ldflags),bcrfile.so

createc_so_SOURCES = createc.c
createc_so_LDFLAGS = $(module_ldflags),createc.so

ezdfile_so_SOURCES = ezdfile.c
ezdfile_so_LDFLAGS = $(module_ldflags),ezdfile.so

gwyfile_so_SOURCES = gwyfile.c
gwyfile_so_LDFLAGS = $(module_ldflags),gwyfile.so

jpkscan_so_SOURCES = jpk.h jpkscan.c
jpkscan_so_LDFLAGS = $(module_ldflags),jpkscan.so

nanoscope_so_SOURCES = nanoscope.c
nanoscope_so_LDFLAGS = $(module_ldflags),nanoscope.so

nt_mdt_so_SOURCES = nt-mdt.c
nt_mdt_so_LDFLAGS = $(module_ldflags),nt-mdt.so

pixmap_so_SOURCES = pixmap.c
pixmap_so_LDFLAGS = $(module_ldflags),pixmap.so

rawfile_so_SOURCES = rawfile.c
rawfile_so_LDFLAGS = $(module_ldflags),rawfile.so

rhk_spm32_so_SOURCES = rhk-spm32.c
rhk_spm32_so_LDFLAGS = $(module_ldflags),rhk-spm32.so

sis_so_SOURCES = sis.c
sis_so_LDFLAGS = $(module_ldflags),sis.so

spmlab_so_SOURCES = spmlab.c
spmlab_so_LDFLAGS = $(module_ldflags),spmlab.so

stmprg_so_SOURCES = stmprg.c
stmprg_so_LDFLAGS = $(module_ldflags),stmprg.so

stpfile_so_SOURCES = stpfile.c
stpfile_so_LDFLAGS = $(module_ldflags),stpfile.so

wsxmfile_so_SOURCES = wsxmfile.c
wsxmfile_so_LDFLAGS = $(module_ldflags),wsxmfile.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\" -fPIC

clean-local:
	rm -f core.* *~
