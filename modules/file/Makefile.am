# @(#) $Id$ */

filedir = $(pkglibdir)/modules/file

module_ldflags = -shared -Wl,-soname

file_PROGRAMS = \
	asciiexport.so \
	createc.so \
	gwyfile.so \
	nanoscope.so \
	nt-mdt.so \
	pixmap.so \
	rawfile.so \
	sis.so

EXTRA_DIST = \
	makefile.msc

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


INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\" -fPIC

clean-local:
	rm -f core.* *~
