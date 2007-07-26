# Generic gtk-doc rules.
# @(#) $Id$
# Included in all API-docs Makefile.am's, if you change anything here, it's
# affects all subdirs.

gtkdocdir = /usr/share/gtk-doc/data

GWY_DOC_CFLAGS = -I$(top_srcdir) -I$(top_builddir) @COMMON_CFLAGS@
GWY_DOC_LIBS = \
	$(top_builddir)/app/libgwyapp2.la \
	$(top_builddir)/libgwymodule/libgwymodule2.la \
	$(top_builddir)/libgwydgets/libgwydgets2.la \
	$(top_builddir)/libdraw/libgwydraw2.la \
	$(top_builddir)/libprocess/libgwyprocess2.la \
	$(top_builddir)/libgwyddion/libgwyddion2.la \
	@GTKGLEXT_LIBS@ @BASIC_LIBS@

GWY_SCAN_OPTIONS = \
	--rebuild-sections --rebuild-types \
	--deprecated-guards="GWY_DISABLE_DEPRECATED" \
	--ignore-decorators="_GWY_STATIC_INLINE"

GTKDOC_CC = $(LIBTOOL) --mode=compile $(CC) $(GWY_DOC_CFLAGS) $(GTKDOC_CFLAGS) $(CPPFLAGS) $(CFLAGS)
GTKDOC_LD = $(LIBTOOL) --mode=link $(CC) $(GWY_DOC_LIBS) $(GTKDOC_LIBS) $(CFLAGS) $(LDFLAGS)

# We set GPATH here; this gives us semantics for GNU make
# which are more like other make's VPATH, when it comes to
# whether a source that is a target of one rule is then
# searched for in VPATH/GPATH.
#
GPATH = $(srcdir)

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)
ADD_OBJECTS = $(top_srcdir)/devel-docs/add-objects.py

EXTRA_DIST = \
	$(content_files) \
	makefile.msc \
	releaseinfo.xml.in \
	$(HTML_IMAGES) \
	$(DOC_MAIN_SGML_FILE) \
	$(DOC_MODULE)-overrides.txt

DOC_STAMPS = \
	scan-build.stamp \
	tmpl-build.stamp \
	sgml-build.stamp \
	html-build.stamp \
	tmpl.stamp \
	sgml.stamp \
	html.stamp

SCANOBJ_FILES = \
	$(DOC_MODULE).args \
	$(DOC_MODULE).hierarchy \
	$(DOC_MODULE).interfaces \
	$(DOC_MODULE).prerequisites \
	$(DOC_MODULE).signals

CLEANFILES = $(SCANOBJ_FILES) $(DOC_MODULE)-unused.txt $(DOC_STAMPS)

DISTCLEANFILES = \
	$(DOC_MODULE)-sections.txt \
	$(DOC_MODULE)-undocumented.txt \
	$(DOC_MODULE)-undeclared.txt \
	$(DOC_MODULE)-decl-list.txt \
	$(DOC_MODULE)-decl.txt \
	$(DOC_MODULE).types

HFILE_GLOB = $(top_srcdir)/$(DOC_SOURCE_DIR)/*.h $(MORE_HFILES)
CFILE_GLOB = $(top_srcdir)/$(DOC_SOURCE_DIR)/*.c $(MORE_CFILES)

if ENABLE_GTK_DOC
# XXX: Uncomment following line to rebuild docs automatically
#all-local: docs

docs: html-build.stamp

#### scan ####

scan-build.stamp: $(HFILE_GLOB) $(CFILE_GLOB) $(ADD_OBJECTS)
	@echo 'gtk-doc: Scanning header files'
	gtkdoc-scan --module=$(DOC_MODULE) \
	            --source-dir=$(top_srcdir)/$(DOC_SOURCE_DIR) \
	            --source-dir=$(top_builddir)/$(DOC_SOURCE_DIR) \
	            $(GWY_SCAN_OPTIONS) $(SCAN_OPTIONS)
	if grep -l '^..*$$' $(DOC_MODULE).types >/dev/null 2>&1 ; then \
		CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" gtkdoc-scangobj $(SCANGOBJ_OPTIONS) --module=$(DOC_MODULE) --output-dir=$(builddir); \
	else \
		for i in $(SCANOBJ_FILES); do \
			test -f $$i || touch $$i ; \
		done \
	fi
	if test -s $(DOC_MODULE).hierarchy; then \
		$(PYTHON) $(ADD_OBJECTS) $(DOC_MODULE)-sections.txt $(DOC_MODULE).hierarchy $(ADDOBJECTS_OPTIONS); \
	fi
	touch scan-build.stamp

$(DOC_MODULE)-decl.txt $(SCANOBJ_FILES): scan-build.stamp
	@true

#### templates ####

tmpl-build.stamp: $(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-overrides.txt
	@echo 'gtk-doc: Rebuilding template files'
	gtkdoc-mktmpl --module=$(DOC_MODULE) --output-dir=template $(MKTMPL_OPTIONS)
	for i in template/*.sgml; do \
		sed '2s/.*//' "$$i" >$(DOC_MODULE).rstmpl; \
		diff "$$i" $(DOC_MODULE).rstmpl >/dev/null 2>&1 \
			|| cat $(DOC_MODULE).rstmpl >"$$i"; \
	done; \
	rm -f $(DOC_MODULE).rstmpl; \
	touch tmpl-build.stamp

tmpl.stamp: tmpl-build.stamp
	@true

#### xml ####

sgml-build.stamp: tmpl.stamp $(CFILE_GLOB) $(expand_content_files)
	@echo 'gtk-doc: Building XML'
	gtkdoc-mkdb --module=$(DOC_MODULE) --tmpl-dir=template \
	            --source-dir=$(top_srcdir)/$(DOC_SOURCE_DIR) \
	            --source-dir=$(top_builddir)/$(DOC_SOURCE_DIR) \
	            --sgml-mode --output-format=xml \
	            --expand-content-files="$(expand_content_files)" \
	            --main-sgml-file=$(DOC_MAIN_SGML_FILE) $(MKDB_OPTIONS)
	touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

# Note the test for Makefile.am is a desperate measure to detect VPATH
# builds.  Comparing srcdir and builddir is not safe as different strings
# can resolve to the same directory.  Giving xsltproc directly
# $(DOC_MAIN_SGML_FILE) in source directory apparently totally messes paths
# so we can't do this.
html-build.stamp: sgml.stamp $(srcdir)/$(DOC_MAIN_SGML_FILE) $(content_files) releaseinfo.xml
	@echo 'gtk-doc: Building HTML'
	rm -rf html
	mkdir html
	test -f Makefile.am || cp $(srcdir)/$(DOC_MAIN_SGML_FILE) .
	test ! -f html/index.sgml || rm -f html/index.sgml
	cd html \
		&& /usr/bin/xsltproc --path $(abs_srcdir) --nonet --xinclude \
		                     --stringparam gtkdoc.bookname $(DOC_MODULE) \
		                     --stringparam gtkdoc.version "1.8" \
		                     $(gtkdocdir)/gtk-doc.xsl \
		                     ../$(DOC_MAIN_SGML_FILE)
	@echo 'gtk-doc: Copying styles and images'
	cd $(gtkdocdir) && cp -f *.png *.css $(abs_builddir)/html/
	test "x$(HTML_IMAGES)" = "x" \
		|| ( cd $(srcdir)/html && cp $(HTML_IMAGES) $(abs_builddir)/html/ )
	@echo 'gtk-doc: Fixing cross-references'
	gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)
	cd $(top_srcdir)/devel-docs && cp style.css $(abs_builddir)/html/
	touch html-build.stamp
else
docs:
endif

##############

clean-local:
	rm -f *~ *.bak $(DOC_MODULE)-scan.*

distclean-local:
	rm -rf xml template
	test -f Makefile.am || rm -f $(DOC_MAIN_SGML_FILE)

maintainer-clean-local:
	rm -rf html

install-data-local:
	d=; \
	if test -s html/index.sgml; then \
		echo 'gtk-doc: Installing HTML from builddir'; \
		d=html; \
	elif test -s $(srcdir)/html/index.sgml; then \
		echo 'gtk-doc: Installing HTML from srcdir'; \
		d=$(srcdir)/html; \
	else \
		echo 'gtk-doc: Nothing to install'; \
	fi; \
	if test -n "$$d"; then \
		$(mkdir_p) $(DESTDIR)$(TARGET_DIR); \
		for i in $$d/*; do \
			$(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
		done; \
	fi; \
	test -n "$$d"

uninstall-local:
	rm -f $(DESTDIR)$(TARGET_DIR)/*
	rmdir $(DESTDIR)$(TARGET_DIR)

#
# Require gtk-doc when making dist
# FIXME: We should require *built* docs, because this is not guaranteed just
# by having gtk-doc enabled.
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-gtkdoc dist-hook-local
	mkdir $(distdir)/html
	if test -s html/index.sgml; then d=html; else d=$(srcdir)/html; fi; \
	cp $$d/* $(distdir)/html
	$(PYTHON) $(top_srcdir)/devel-docs/ncrosslinks.py $(distdir)/html/*.html </dev/null

.PHONY: docs dist-hook-local
