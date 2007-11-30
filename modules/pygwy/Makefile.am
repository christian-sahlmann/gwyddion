# @(#) $Id$

moduledir = $(pkglibdir)/modules

PYGWY_H_FILES = \
	$(top_srcdir)/libprocess/arithmetic.h \
	$(top_srcdir)/libprocess/cdline.h \
	$(top_srcdir)/libprocess/correct.h \
	$(top_srcdir)/libprocess/correlation.h \
	$(top_srcdir)/libprocess/cwt.h \
	$(top_srcdir)/libprocess/datafield.h \
	$(top_srcdir)/libprocess/dataline.h \
	$(top_srcdir)/libprocess/dwt.h \
	$(top_srcdir)/libprocess/elliptic.h \
	$(top_srcdir)/libprocess/filters.h \
	$(top_srcdir)/libprocess/fractals.h \
	$(top_srcdir)/libprocess/grains.h \
	$(top_srcdir)/libprocess/gwyprocessenums.h \
	$(top_srcdir)/libprocess/gwyprocess.h \
	$(top_srcdir)/libprocess/gwyprocesstypes.h \
	$(top_srcdir)/libprocess/hough.h \
	$(top_srcdir)/libprocess/interpolation.h \
	$(top_srcdir)/libprocess/inttrans.h \
	$(top_srcdir)/libprocess/level.h \
	$(top_srcdir)/libprocess/linestats.h \
	$(top_srcdir)/libprocess/simplefft.h \
	$(top_srcdir)/libprocess/spectra.h \
	$(top_srcdir)/libprocess/stats.h \
	$(top_srcdir)/libprocess/tip.h \
	$(top_srcdir)/libgwyddion/gwycontainer.h \
	$(top_srcdir)/libgwyddion/gwyddionenums.h \
	$(top_srcdir)/libgwyddion/gwyddion.h \
	$(top_srcdir)/libgwyddion/gwyddiontypes.h \
	$(top_srcdir)/libgwyddion/gwyentities.h \
	$(top_srcdir)/libgwyddion/gwyenum.h \
	$(top_srcdir)/libgwyddion/gwyexpr.h \
	$(top_srcdir)/libgwyddion/gwyfdcurvepreset.h \
	$(top_srcdir)/libgwyddion/gwyinventory.h \
	$(top_srcdir)/libgwyddion/gwymacros.h \
	$(top_srcdir)/libgwyddion/gwymath.h \
	$(top_srcdir)/libgwyddion/gwymd5.h \
	$(top_srcdir)/libgwyddion/gwynlfit.h \
	$(top_srcdir)/libgwyddion/gwynlfitpreset.h \
	$(top_srcdir)/libgwyddion/gwyresource.h \
	$(top_srcdir)/libgwyddion/gwyserializable.h \
	$(top_srcdir)/libgwyddion/gwysiunit.h \
	$(top_srcdir)/libgwyddion/gwystringlist.h \
	$(top_srcdir)/libgwyddion/gwyutils.h \
	$(top_srcdir)/libgwyddion/gwyversion.h \
	$(top_srcdir)/app/app.h \
	$(top_srcdir)/app/data-browser.h \
	$(top_srcdir)/app/datachooser.h \
	$(top_srcdir)/app/file.h \
	$(top_srcdir)/app/filelist.h \
	$(top_srcdir)/app/funcuse.h \
	$(top_srcdir)/app/glmaterial-editor.h \
	$(top_srcdir)/app/gradient-editor.h \
	$(top_srcdir)/app/gwyapp.h \
	$(top_srcdir)/app/gwyapptypes.h \
	$(top_srcdir)/app/gwymoduleutils-file.h \
	$(top_srcdir)/app/gwymoduleutils.h \
	$(top_srcdir)/app/gwyplaintool.h \
	$(top_srcdir)/app/menu.h \
	$(top_srcdir)/app/resource-editor.h \
	$(top_srcdir)/app/settings.h \
	$(top_srcdir)/app/undo.h \
	$(top_srcdir)/app/wait.h \
	$(top_srcdir)/libdraw/gwydraw.h \
	$(top_srcdir)/libdraw/gwyglmaterial.h \
	$(top_srcdir)/libdraw/gwygradient.h \
	$(top_srcdir)/libdraw/gwypixfield.h \
	$(top_srcdir)/libdraw/gwyrgba.h \
	$(top_srcdir)/libdraw/gwyselection.h \
	$(top_srcdir)/libgwymodule/gwymodulebrowser.h \
	$(top_srcdir)/libgwymodule/gwymoduleenums.h \
	$(top_srcdir)/libgwymodule/gwymodule-file.h \
	$(top_srcdir)/libgwymodule/gwymodule-graph.h \
	$(top_srcdir)/libgwymodule/gwymodule.h \
	$(top_srcdir)/libgwymodule/gwymodule-layer.h \
	$(top_srcdir)/libgwymodule/gwymoduleloader.h \
	$(top_srcdir)/libgwymodule/gwymodule-process.h \
	$(top_srcdir)/libgwymodule/gwymodule-tool.h \
	$(top_srcdir)/libgwymodule/gwymoduletypes.h \
	$(top_srcdir)/libgwydgets/gwy3dlabel.h \
	$(top_srcdir)/libgwydgets/gwy3dsetup.h \
	$(top_srcdir)/libgwydgets/gwy3dview.h \
	$(top_srcdir)/libgwydgets/gwy3dwindow.h \
	$(top_srcdir)/libgwydgets/gwyaxis.h \
	$(top_srcdir)/libgwydgets/gwycoloraxis.h \
	$(top_srcdir)/libgwydgets/gwycolorbutton.h \
	$(top_srcdir)/libgwydgets/gwycombobox.h \
	$(top_srcdir)/libgwydgets/gwycurve.h \
	$(top_srcdir)/libgwydgets/gwydataview.h \
	$(top_srcdir)/libgwydgets/gwydataviewlayer.h \
	$(top_srcdir)/libgwydgets/gwydatawindow.h \
	$(top_srcdir)/libgwydgets/gwydgetenums.h \
	$(top_srcdir)/libgwydgets/gwydgetmarshals.h \
	$(top_srcdir)/libgwydgets/gwydgets.h \
	$(top_srcdir)/libgwydgets/gwydgettypes.h \
	$(top_srcdir)/libgwydgets/gwydgetutils.h \
	$(top_srcdir)/libgwydgets/gwygrapharea.h \
	$(top_srcdir)/libgwydgets/gwygraphbasics.h \
	$(top_srcdir)/libgwydgets/gwygraphcorner.h \
	$(top_srcdir)/libgwydgets/gwygraphcurvemodel.h \
	$(top_srcdir)/libgwydgets/gwygraphcurves.h \
	$(top_srcdir)/libgwydgets/gwygraphdata.h \
	$(top_srcdir)/libgwydgets/gwygraph.h \
	$(top_srcdir)/libgwydgets/gwygraphlabel.h \
	$(top_srcdir)/libgwydgets/gwygraphmodel.h \
	$(top_srcdir)/libgwydgets/gwygraphselections.h \
	$(top_srcdir)/libgwydgets/gwygraphwindow.h \
	$(top_srcdir)/libgwydgets/gwyhmarkerbox.h \
	$(top_srcdir)/libgwydgets/gwyhruler.h \
	$(top_srcdir)/libgwydgets/gwyinventorystore.h \
	$(top_srcdir)/libgwydgets/gwylayer-basic.h \
	$(top_srcdir)/libgwydgets/gwylayer-mask.h \
	$(top_srcdir)/libgwydgets/gwymarkerbox.h \
	$(top_srcdir)/libgwydgets/gwynullstore.h \
	$(top_srcdir)/libgwydgets/gwyoptionmenus.h \
	$(top_srcdir)/libgwydgets/gwypixmaplayer.h \
	$(top_srcdir)/libgwydgets/gwyradiobuttons.h \
	$(top_srcdir)/libgwydgets/gwyruler.h \
	$(top_srcdir)/libgwydgets/gwyscitext.h \
	$(top_srcdir)/libgwydgets/gwyselectiongraph1darea.h \
	$(top_srcdir)/libgwydgets/gwyselectiongrapharea.h \
	$(top_srcdir)/libgwydgets/gwyselectiongraphline.h \
	$(top_srcdir)/libgwydgets/gwyselectiongraphpoint.h \
	$(top_srcdir)/libgwydgets/gwyselectiongraphzoom.h \
	$(top_srcdir)/libgwydgets/gwysensitivitygroup.h \
	$(top_srcdir)/libgwydgets/gwyshader.h \
	$(top_srcdir)/libgwydgets/gwystatusbar.h \
	$(top_srcdir)/libgwydgets/gwystock.h \
	$(top_srcdir)/libgwydgets/gwyvectorlayer.h \
	$(top_srcdir)/libgwydgets/gwyvruler.h 

#	$(top_srcdir)/app/gwytool.h 
#	$(top_srcdir)/libgwyddion/gwydebugobjects.h 
#	$(top_srcdir)/libdraw/gwydrawenums.h 

#	$(top_srcdir)/libgwymodule/gwymodule-file.h 
#	$(top_srcdir)/libgwymodule/gwymoduleenums.h 

#	$(top_srcdir)/libgwyddion/gwysiunit.h 
#	$(top_srcdir)/libgwyddion/gwyenum.h 
#	$(top_srcdir)/libgwyddion/gwyresource.h 
#	$(top_srcdir)/libgwyddion/gwycontainer.h 
#	$(top_srcdir)/app/data-browser.h 
#	$(top_srcdir)/app/undo.h 


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

AM_CPPFLAGS = -I$(top_srcdir) -DG_LOG_DOMAIN=\"Module\"
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

pygwy.defs: $(PYGWY_H_FILES) Makefile.am
	$(PYTHON) $(PYGTK_CODEGENDIR)/h2def.py $(PYGWY_H_FILES) \
	| sed -e 's/define-object Serializable/define-interface Serializable/' -e 's/GWY_TYPE_SELECTION_GRAPH1_DAREA/GWY_TYPE_SELECTION_GRAPH_1DAREA/' -e 's/GWY_TYPE_NL_FIT_PRESET/GWY_TYPE_NLFIT_PRESET/' -e 's/GWY_TYPE_CD_LINE/GWY_TYPE_CDLINE/' > pygwy.defs

# The sed fixes pygwy_add_constants() and pygwy_register_classes() to static
# functions, and standardized `generated' line for automated exclusions of
# generated files.
# Keep the `GENERATED' string quoted to prevent match here
pygwywrap.c: $(srcdir)/pygwy-codegen.py pygwy.defs $(srcdir)/pygwy.override
	$(PYTHON) $(srcdir)/pygwy-codegen.py --codegendir $(PYGTK_CODEGENDIR) \
	  --override $(srcdir)/pygwy.override --prefix pygwy pygwy.defs \
	  | sed -e 's/^void$$/static void/' \
	        -e '1s#.*#/* This is a 'GENERATED' file */#' >pygwywrap.c

pygwy-doc: pygwy.defs pygwywrap.c
	$(PYTHON) ./pygwy-generate-doc.py --codegendir $(PYGTK_CODEGENDIR) >gwy.py
	$(EPYDOC) -o pygwy-doc gwy.py
endif

CLEANFILES = gwy.py gwy.pyc gwy.pyo

DISTCLEANFILES = pygwy.defs pygwywrap.c

MAINTAINERCLEANFILES = makefile.msc

clean-local:
	rm -f core.* *~
	rm -rf pygwy-doc/
