# @(#) $Id$ */

filedir = $(gwymoduledir)/file

module_ldflags = -shared -Wl,-soname

file_PROGRAMS = \
	gwyfile.so \
	nanoscope.so \
	pixmap.so \
	rawfile.so

EXTRA_DIST = \
	makefile.msc

gwyfile_so_SOURCES = gwyfile.c
gwyfile_so_LDFLAGS = $(module_ldflags),gwyfile.so

pixmap_so_SOURCES = pixmap.c
pixmap_so_LDFLAGS = $(module_ldflags),pixmap.so

rawfile_so_SOURCES = rawfile.c
rawfile_so_LDFLAGS = $(module_ldflags),rawfile.so

nanoscope_so_SOURCES = nanoscope.c
nanoscope_so_LDFLAGS = $(module_ldflags),nanoscope.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
