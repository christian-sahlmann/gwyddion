# @(#) $Id$ */

filedir = $(gwymoduledir)/file

module_ldflags = -shared -Wl,-soname

file_PROGRAMS = \
	gwyfile.so \
	pixmap.so \
	plugin-proxy.so

gwyfile_so_SOURCES = gwyfile.c
gwyfile_so_LDFLAGS = $(module_ldflags),gwyfile.so

plugin_proxy_so_SOURCES = plugin-proxy.c
plugin_proxy_so_LDFLAGS = $(module_ldflags),plugin-proxy.so

pixmap_so_SOURCES = pixmap.c
pixmap_so_LDFLAGS = $(module_ldflags),pixmap.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Module\"

clean-local:
	rm -f core.* *~
