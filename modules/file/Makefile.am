# @(#) $Id$ */

filedir = $(pkglibdir)/modules/file

module_ldflags = -shared -Wl,-soname

file_PROGRAMS = \
	asciiexport.so \
	assing-afm.so \
	createc.so \
	gwyfile.so \
	nanoscope.so \
	nt-mdt.so \
	pixmap.so \
	rawfile.so \
	sis.so \
	spmlab.so \
	stmprg.so

noinst_HEADERS = \
	get.h \
	stmprg.h

EXTRA_DIST = \
	makefile.msc

assing_afm_so_SOURCES = assing-afm.c
assing_afm_so_LDFLAGS = $(module_ldflags),assing-afm.so

asciiexport_so_SOURCES = asciiexport.c
asciiexport_so_LDFLAGS = $(module_ldflags),asciiexport.so

createc_so_SOURCES = createc.c
createc_so_LDFLAGS = $(module_ldflags),createc.so

gwyfile_so_SOURCES = gwyfile.c
gwyfile_so_LDFLAGS = $(module_ldflags),gwyfile.so

nanoscope_so_SOURCES = nanoscope.c
nanoscope_so_LDFLAGS = $(module_ldflags),nanoscope.so

nt_mdt_so_SOURCES = nt-mdt.c
nt_mdt_so_LDFLAGS = $(module_ldflags),nt-mdt.so

pixmap_so_SOURCES = pixmap.c
pixmap_so_LDFLAGS = $(module_ldflags),pixmap.so

rawfile_so_SOURCES = rawfile.c
rawfile_so_LDFLAGS = $(module_ldflags),rawfile.so

sis_so_SOURCES = sis.c
sis_so_LDFLAGS = $(module_ldflags),sis.so

spmlab_so_SOURCES = spmlab.c
spmlab_so_LDFLAGS = $(module_ldflags),spmlab.so

stmprg_so_SOURCES = stmprg.c
stmprg_so_LDFLAGS = $(module_ldflags),stmprg.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\" -fPIC

clean-local:
	rm -f core.* *~
