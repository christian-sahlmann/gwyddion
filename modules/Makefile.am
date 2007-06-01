# @(#) $Id$

PYGWY_H_FILES = ../libprocess/*.h ../libgwyddion/gwysiunit.h ../libgwyddion/gwyenum.h ../libgwyddion/gwyresource.h ../app/data-browser.h ../libgwyddion/gwycontainer.h ../app/undo.h ../libdraw/gwyselection.h

SUBDIRS = \
	file \
	graph \
	layer \
	process \
	tools

moduledir = $(pkglibdir)/modules

if ENABLE_PLUGIN_PROXY
plugin_proxy_module = plugin-proxy.la
plugin_proxy_sources =
else
plugin_proxy_module =
plugin_proxy_sources = plugin-proxy.c
endif

if ENABLE_PYGWY
pygwy_module = pygwy.la
pygwy_sources =
else
pygwy_module =
pygwy_sources = pygwy.c
endif

module_LTLIBRARIES = \
	$(plugin_proxy_module) \
	$(pygwy_module)

EXTRA_DIST = \
	$(plugin_proxy_sources) \
	$(pygwy_sources) \
	pygwy.override \
	pygwy-codegen.py \
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
	$(top_builddir)/app/libgwyapp2.la \
	$(top_builddir)/libdraw/libgwydraw2.la \
	$(top_builddir)/libgwyddion/libgwyddion2.la \
	$(top_builddir)/libgwydgets/libgwydgets2.la \
	$(top_builddir)/libgwymodule/libgwymodule2.la \
	$(top_builddir)/libprocess/libgwyprocess2.la \
	@GTK_LIBS@
endif


if ENABLE_PLUGIN_PROXY
plugin_proxy_la_SOURCES = plugin-proxy.c
plugin_proxy_la_LIBADD = $(module_libadd)
endif

if ENABLE_PYGWY
BUILT_SOURCES = pygwywrap.c
pygwy_la_SOURCES = pygwy.c
pygwy_la_CFLAGS = @COMMON_CFLAGS@ @WARNING_NOUNUSED_PAR@ $(PYTHON_INCLUDES) $(PYGTK_CFLAGS)
pygwy_la_LIBADD = $(module_libadd) $(PYTHON_LIBS) $(PYGTK_LIBS)

pygwy.defs: $(PYGWY_H_FILES)
	$(PYTHON) $(PYGTK_CODEGENDIR)/h2def.py $(PYGWY_H_FILES) >pygwy.defs

# The sed fixes pygwy_add_constants() and pygwy_register_classes() to static
# functions.
pygwywrap.c: pygwy-codegen.py pygwy.defs pygwy.override
	$(PYTHON) ./pygwy-codegen.py --codegendir $(PYGTK_CODEGENDIR) \
	  --override pygwy.override --prefix pygwy pygwy.defs \
	  | sed -e 's/^void$$/static void/' >pygwywrap.c
endif

MAINTAINERCLEANFILES = makefile.msc

DISTCLEANFILES = pygwy.defs pygwywrap.c

clean-local:
	rm -f core.* *~
