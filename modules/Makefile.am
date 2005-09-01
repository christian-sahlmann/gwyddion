# @(#) $Id$

SUBDIRS = \
	file \
	graph \
	layer \
	process \
	tools

moduledir = $(pkglibdir)/modules

module_LTLIBRARIES = \
	plugin-proxy.la


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
module_libadd = \
	$(top_builddir)/app/libgwyapp.la		\
	$(top_builddir)/libdraw/libgwydraw.la		\
	$(top_builddir)/libgwyddion/libgwyddion.la	\
	$(top_builddir)/libgwydgets/libgwydgets.la	\
	$(top_builddir)/libgwymodule/libgwymodule.la	\
	$(top_builddir)/libprocess/libgwyprocess.la	\
	@GTK_LIBS@
endif


plugin_proxy_la_SOURCES = plugin-proxy.c
plugin_proxy_la_LIBADD = $(module_libadd)


# To run uninstalled
all-local: tool

tool:
	$(LN_S) tools tool

clean-local:
	rm -f core.* *~ tool
