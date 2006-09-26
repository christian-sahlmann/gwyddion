# Generic gtk-doc rules.
# @(#) $Id$
# Included in all API-docs Makefile.am's, if you change anything here, it's
# affects all subdirs.

if GTK_DOC_USE_LIBTOOL
GTKDOC_CC = $(LIBTOOL) --mode=compile $(CC) $(INCLUDES) $(AM_CFLAGS) $(CFLAGS)
GTKDOC_LD = $(LIBTOOL) --mode=link $(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS)
else
GTKDOC_CC = $(CC) $(INCLUDES) $(AM_CFLAGS) $(CFLAGS)
GTKDOC_LD = $(CC) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS)
endif

# We set GPATH here; this gives us semantics for GNU make
# which are more like other make's VPATH, when it comes to
# whether a source that is a target of one rule is then
# searched for in VPATH/GPATH.
#
GPATH = $(srcdir)

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

EXTRA_DIST = \
	$(content_files) \
	makefile.msc \
	$(HTML_IMAGES) \
	$(DOC_MAIN_SGML_FILE) \
	$(DOC_MODULE)-sections.txt \
	$(DOC_MODULE)-decl.txt \
	$(DOC_MODULE).types \
	$(DOC_MODULE)-overrides.txt

DOC_STAMPS = \
	scan-build.stamp \
	tmpl-build.stamp \
	sgml-build.stamp \
	html-build.stamp \
	$(srcdir)/tmpl.stamp \
	$(srcdir)/sgml.stamp \
	$(srcdir)/html.stamp

SCANOBJ_FILES = \
	$(DOC_MODULE).args \
	$(DOC_MODULE).hierarchy \
	$(DOC_MODULE).interfaces \
	$(DOC_MODULE).prerequisites \
	$(DOC_MODULE).signals

CLEANFILES = $(SCANOBJ_FILES) $(DOC_MODULE)-unused.txt $(DOC_STAMPS)

HFILE_GLOB = $(DOC_SOURCE_DIR)/*.h
CFILE_GLOB = $(DOC_SOURCE_DIR)/*.c

if ENABLE_GTK_DOC
# XXX: Uncomment following line to rebuild docs automatically
#all-local: docs

docs: html-build.stamp

#### scan ####

scan-build.stamp: $(HFILE_GLOB) $(CFILE_GLOB)
	@echo 'gtk-doc: Scanning header files'
	@-chmod -R u+w $(srcdir)
	if test "x$(TYPES_INCLUDE)" != x; then \
	    echo "$(TYPES_INCLUDE)"; \
	    IGNORE_HFILES="$(IGNORE_HFILES)" $(top_srcdir)/devel-docs/extract-types.py $(HFILE_GLOB); \
	fi >$(srcdir)/$(DOC_MODULE).types
	if grep -l '^..*$$' $(srcdir)/$(DOC_MODULE).types >/dev/null 2>&1 ; then \
	    CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" CFLAGS="$(GTKDOC_CFLAGS)" LDFLAGS="$(GTKDOC_LIBS)" gtkdoc-scangobj $(SCANGOBJ_OPTIONS) --module=$(DOC_MODULE) --output-dir=$(srcdir); \
	else \
	    cd $(srcdir) ; \
	    for i in $(SCANOBJ_FILES); do \
               test -f $$i || touch $$i ; \
	    done \
	fi
	cd $(srcdir) && \
	  gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR) --ignore-headers="$(IGNORE_HFILES)" $(SCAN_OPTIONS) $(EXTRA_HFILES)
	if test -s $(srcdir)/$(DOC_MODULE).hierarchy; then \
		${top_srcdir}/devel-docs/add-objects.py $(srcdir)/$(DOC_MODULE)-sections.txt $(srcdir)/$(DOC_MODULE).hierarchy; \
	fi
	touch scan-build.stamp

$(DOC_MODULE)-decl.txt $(SCANOBJ_FILES): scan-build.stamp
	@true

#### templates ####

tmpl-build.stamp: $(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_MODULE)-overrides.txt
	@echo 'gtk-doc: Rebuilding template files'
	@-chmod -R u+w $(srcdir)
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE) --output-dir=$(srcdir)/template $(MKTMPL_OPTIONS)
	for i in $(srcdir)/template/*.sgml; do \
	  sed '2s/.*//' "$$i" >$(DOC_MODULE).rstmpl; \
	    if diff "$$i" $(DOC_MODULE).rstmpl >/dev/null 2>&1; then :; else \
	      cat $(DOC_MODULE).rstmpl >"$$i"; \
	    fi; \
	  done; \
	rm -f $(DOC_MODULE).rstmpl; \
	touch tmpl-build.stamp

tmpl.stamp: tmpl-build.stamp
	@true

#### xml ####

sgml-build.stamp: tmpl.stamp $(CFILE_GLOB) $(srcdir)/template/*.sgml $(expand_content_files)
	@echo 'gtk-doc: Building XML'
	@-chmod -R u+w $(srcdir)
	cd $(srcdir) && \
	gtkdoc-mkdb --module=$(DOC_MODULE) --tmpl-dir=$(srcdir)/template --source-dir=$(DOC_SOURCE_DIR) --sgml-mode --output-format=xml --expand-content-files="$(expand_content_files)" --main-sgml-file=$(DOC_MAIN_SGML_FILE) $(MKDB_OPTIONS)
	touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

html-build.stamp: sgml.stamp $(DOC_MAIN_SGML_FILE) $(content_files)
	@echo 'gtk-doc: Building HTML'
	@-chmod -R u+w $(srcdir)
	rm -rf $(srcdir)/html
	mkdir $(srcdir)/html
	cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	test "x$(HTML_IMAGES)" = "x" || ( cd $(srcdir) && cp $(HTML_IMAGES) html )
	@echo 'gtk-doc: Fixing cross-references'
	cd $(srcdir) && gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)
	touch html-build.stamp
else
docs:
endif

##############

clean-local:
	rm -f *~ *.bak
	rm -rf .libs

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf xml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt
	cd $(srcdir) && rm -rf template $(DOC_MODULE).types $(DOC_MODULE)-sections.txt

install-data-local:
	installfiles=`echo $(srcdir)/html/*`; \
	if test "$$installfiles" = '$(srcdir)/html/*'; \
	then echo 'gtk-doc: Nothing to install' ; \
	else \
	  $(mkdir_p) $(DESTDIR)$(TARGET_DIR); \
	  for i in $$installfiles; do \
	    echo 'gtk-doc: Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR) || :; \
	fi

uninstall-local:
	rm -f $(DESTDIR)$(TARGET_DIR)/*
	rmdir $(DESTDIR)$(TARGET_DIR)

#
# Require gtk-doc when making dist
#
if ENABLE_GTK_DOC
dist-check-gtkdoc:
else
dist-check-gtkdoc:
	@echo "*** gtk-doc must be installed and enabled in order to make dist"
	@false
endif

dist-hook: dist-check-gtkdoc dist-hook-local
	mkdir $(distdir)/template
	mkdir $(distdir)/xml
	mkdir $(distdir)/html
	-cp $(srcdir)/template/*.sgml $(distdir)/template
	-cp $(srcdir)/xml/*.xml $(distdir)/xml
	-cp $(srcdir)/html/* $(distdir)/html
	if test -f $(srcdir)/$(DOC_MODULE).types; then \
	  cp $(srcdir)/$(DOC_MODULE).types $(distdir)/$(DOC_MODULE).types; \
	fi

.PHONY: docs dist-hook-local
