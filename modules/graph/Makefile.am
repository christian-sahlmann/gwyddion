# @(#) $Id$

graphdir = $(pkglibdir)/modules/graph

module_ldflags = -shared -Wl,-soname

 graph_PROGRAMS = \
 	graph_fit.so \
	graph_cd.so
 

EXTRA_DIST = \
	makefile.msc


#graph_export_ascii_so_SOURCES = graph_export_ascii.c
#graph_export_ascii_so_LDFLAGS = $(module_ldflags),graph_export_ascii.so

graph_fit_so_SOURCES = graph_fit.c
graph_fit_so_LDFLAGS = $(module_ldflags),graph_fit.so

graph_cd_so_SOURCES = graph_cd.c
graph_cd_so_LDFLAGS = $(module_ldflags),graph_cd.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\" -fPIC

clean-local:
	rm -f core.* *~
