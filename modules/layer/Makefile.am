# @(#) $Id$

layerdir = $(gwymoduledir)/tool

module_ldflags = -shared -Wl,-soname

layer_PROGRAMS = \
	axes.so \
	lines.so \
	pointer.so \
	points.so \
	select.so

EXTRA_DIST = \
	makefile.msc

axes_so_SOURCES = axes.c
axes_so_LDFLAGS = $(module_ldflags),axes.so

lines_so_SOURCES = lines.c
lines_so_LDFLAGS = $(module_ldflags),lines.so

pointer_so_SOURCES = pointer.c
pointer_so_LDFLAGS = $(module_ldflags),pointer.so

points_so_SOURCES = points.c
points_so_LDFLAGS = $(module_ldflags),points.so

select_so_SOURCES = select.c
select_so_LDFLAGS = $(module_ldflags),select.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
