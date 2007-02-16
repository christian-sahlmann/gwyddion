# @(#) $Id$

PYGWY_H_FILES = ../libprocess/*.h ../libgwyddion/gwysiunit.h ../libgwyddion/gwyenum.h ../libgwyddion/gwyresource.h ../app/data-browser.h ../libgwyddion/gwycontainer.h ../app/undo.h

SUBDIRS = \
	file \
	graph \
	layer \
	process \
	tools

moduledir = $(pkglibdir)/modules

if ENABLE_PLUGIN_PROXY
plugin_proxy_la = plugin-proxy.la
plugin_proxy_sources =
else
plugin_proxy_la =
plugin_proxy_sources = plugin-proxy.c
endif

if ENABLE_PYGWY
pygwy_la = pygwy.la
pygwy_sources =
else
pygwy_la =
pygwy_sources = pygwy.c
endif

module_LTLIBRARIES = \
	$(plugin_proxy_la) \
	$(pygwy_la)

EXTRA_DIST = \
	$(plugin_proxy_sources) \
	$(pygwy_sources) \
	pygwy.override \
	makefile.msc \
	makefile.msc.gwt

if OS_WIN32
no_undefined = -no-undefined
endif

AM_CPPFLAGS = -I$(top_buildir) -DG_LOG_DOMAIN=\"Module\"
AM_CFLAGS = @COMMON_CFLAGS@
AM_LDFLAGS = -avoid-version -module $(no_undefined)

if MODULE_DEPENDENCIES
module_libadd = \
	$(top_builddir)/app/libgwyapp2.la		\
	$(top_builddir)/libdraw/libgwydraw2.la		\
	$(top_builddir)/libgwyddion/libgwyddion2.la	\
	$(top_builddir)/libgwydgets/libgwydgets2.la	\
	$(top_builddir)/libgwymodule/libgwymodule2.la	\
	$(top_builddir)/libprocess/libgwyprocess2.la	\
	@GTK_LIBS@
endif


if ENABLE_PLUGIN_PROXY
plugin_proxy_la_SOURCES = plugin-proxy.c
plugin_proxy_la_LIBADD = $(module_libadd)
endif

if ENABLE_PYGWY
BUILT_SOURCES = pygwywrap.c
pygwy_la_SOURCES = pygwy.c
pygwy_la_CFLAGS = @COMMON_CFLAGS@ $(PYTHON_INCLUDES) $(PYGTK_CFLAGS) $(PYGOBJECT_CFLAGS)
pygwy_la_LIBADD = $(module_libadd) $(PYTHON_LIBS) $(PYGOBJECT_LIBS)

pygwy.defs: $(PYGWY_H_FILES)
	$(PYTHON) $(PYGTK_CODEGENDIR)/h2def.py $(PYGWY_H_FILES) > pygwy.defs

pygwywrap.c: pygwy.defs pygwy.override
	pygtk-codegen-2.0 --override pygwy.override --prefix pygwy pygwy.defs > pygwywrap.c
endif

MAINTAINERCLEANFILES = makefile.msc

DISTCLEANFILES = pygwy.defs pygwywrap.c

clean-local:
	rm -f core.* *~
