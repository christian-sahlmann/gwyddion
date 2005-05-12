# @(#) $Id$

moduledir = $(pkglibdir)/modules

module_ldflags = -shared -Wl,-soname

SUBDIRS = \
	file \
	graph \
	layer \
	process \
	tools

module_PROGRAMS = \
	plugin-proxy.so

EXTRA_DIST = \
	makefile.msc

plugin_proxy_so_SOURCES = plugin-proxy.c
plugin_proxy_so_LDFLAGS = $(module_ldflags),plugin-proxy.so

INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DG_LOG_DOMAIN=\"Module\" -fPIC

# To run uninstalled
all-local: tool

tool:
	$(LN_S) tools tool

clean-local:
	rm -f core.* *~ tool
