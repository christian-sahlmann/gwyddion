## Process this file with automake to produce Makefile.in

# This is a blank Makefile.am for using gtk-doc.
# Copy this to your project's API docs directory and modify the variables to
# suit your project. See the GTK+ Makefiles in gtk+/docs/reference for examples
# of using the various options.

# The name of the module, e.g. 'glib'.
DOC_MODULE = libgwydgets
# A misguided directory name...
DOC_DIRNAME = $(DOC_MODULE)

# The top-level SGML file. You can change this if you want to.
DOC_MAIN_SGML_FILE = $(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir).
# gtk-doc will search all .c & .h files beneath here for inline comments
# documenting the functions and macros.
# e.g. DOC_SOURCE_DIR=../../../gtk
DOC_SOURCE_DIR = $(top_srcdir)/$(DOC_DIRNAME)

# Extra options to pass to gtkdoc-scangobj. Not normally needed.
SCANGOBJ_OPTIONS =

# Extra options to pass to add-objects.py
ADDOBJECTS_OPTIONS = --standard-files=gwydgettypes

# Extra options to supply to gtkdoc-scan.
# e.g. SCAN_OPTIONS=--deprecated-guards="GTK_DISABLE_DEPRECATED" 
SCAN_OPTIONS =

# Extra options to supply to gtkdoc-mkdb.
# e.g. MKDB_OPTIONS=--sgml-mode --output-format=xml
MKDB_OPTIONS = --default-includes=libgwydgets/gwydgets.h

# Extra options to supply to gtkdoc-mktmpl
# e.g. MKTMPL_OPTIONS=--only-section-tmpl
MKTMPL_OPTIONS=

# Extra options to supply to gtkdoc-fixref. Not normally needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS = \
	--extra-dir=$(srcdir)/../libgwyddion/html \
	--extra-dir=$(srcdir)/../libgwyprocess/html \
	--extra-dir=$(srcdir)/../libgwydraw/html \
	--extra-dir=$(srcdir)/../libgwymodule/html \
	--extra-dir=$(srcdir)/../libgwyapp/html

# Images to copy into HTML directory.
# e.g. HTML_IMAGES=$(top_srcdir)/gtk/stock-icons/stock_about_24.png
HTML_IMAGES = \
	$(top_srcdir)/pixmaps/gwy_*-[1-9][0-9].png \
	$(top_srcdir)/pixmaps/cd_line.png \
	$(top_srcdir)/pixmaps/cd_rline.png \
	$(top_srcdir)/pixmaps/cd_step.png \
	$(top_srcdir)/pixmaps/cd_rstep.png

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE).
# e.g. content_files=running.sgml building.sgml changes-2.0.sgml
content_files = \
	$(srcdir)/standard-vector-layers.xml

# SGML files where gtk-doc abbrevations (#GtkWidget) are expanded
# These files must be listed here *and* in content_files
# e.g. expand_content_files=running.sgml
expand_content_files=

# This includes the standard gtk-doc make rules.
include $(top_srcdir)/devel-docs/gtk-doc.mk

# Other files to distribute
# e.g. EXTRA_DIST += version.xml.in
EXTRA_DIST +=
