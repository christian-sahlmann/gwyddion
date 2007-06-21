# @(#) $Id$

moduledir = $(pkglibdir)/modules

PYGWY_H_FILES = \
	$(top_srcdir)/libprocess/*.h \
	$(top_srcdir)/libgwyddion/gwysiunit.h \
	$(top_srcdir)/libgwyddion/gwyenum.h \
	$(top_srcdir)/libgwyddion/gwyresource.h \
	$(top_srcdir)/app/data-browser.h \
	$(top_srcdir)/libgwyddion/gwycontainer.h \
	$(top_srcdir)/app/undo.h \
	$(top_srcdir)/libdraw/gwyselection.h 

if ENABLE_PYGWY
pygwy_module = pygwy.la
pygwy_sources =
else
pygwy_module =
pygwy_sources = pygwy.c
endif

module_LTLIBRARIES = \
	$(pygwy_module)

EXTRA_DIST = \
	$(pygwy_sources) \
	README.pygwy \
	pygwy.override \
	pygwy-codegen.py \
	pygwy-generate-doc.py \
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


if ENABLE_PYGWY
BUILT_SOURCES = pygwywrap.c
pygwy_la_SOURCES = pygwy.c
pygwy_la_CFLAGS = @COMMON_CFLAGS@ @WARNING_NOUNUSED_PAR@ $(PYTHON_INCLUDES) $(PYGTK_CFLAGS)
pygwy_la_LIBADD = $(module_libadd) $(PYTHON_LIBS) $(PYGTK_LIBS)

pygwy.defs: $(PYGWY_H_FILES)
	$(PYTHON) $(PYGTK_CODEGENDIR)/h2def.py $(PYGWY_H_FILES) >pygwy.defs

# The sed fixes pygwy_add_constants() and pygwy_register_classes() to static
# functions, and standardized `generated' line for automated exclusions of
# generated files.
# Keep the `GENERATED' string quoted to prevent match here
pygwywrap.c: pygwy-codegen.py pygwy.defs pygwy.override
	$(PYTHON) ./pygwy-codegen.py --codegendir $(PYGTK_CODEGENDIR) \
	  --override pygwy.override --prefix pygwy pygwy.defs \
	  | sed -e 's/^void$$/static void/' \
	        -e '1s#.*#/* This is a 'GENERATED' file */#' >pygwywrap.c

pygwy-doc: pygwy.defs pygwywrap.c
	$(PYTHON) ./pygwy-generate-doc.py --codegendir $(PYGTK_CODEGENDIR) >gwy.py
	$(EPYDOC) -o pygwy-doc gwy.py
endif

CLEANFILES = gwy.py gwy.pyc

DISTCLEANFILES = pygwy.defs pygwywrap.c

MAINTAINERCLEANFILES = makefile.msc

clean-local:
	rm -f core.* *~
	rm -rf pygwy-doc/
