# Generic gtk-doc rules.
# @(#) $Id$
# Included in all API-docs Makefile.am's, if you change anything here, it's
# affects all subdirs.

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

EXTRA_DIST = \
	$(content_files) \
	$(extra_files) \
	$(HTML_IMAGES) \
	$(DOC_MAIN_SGML_FILE) \
	$(DOC_MODULE)-sections.txt \
	$(DOC_MODULE)-decl.txt \
	$(DOC_MODULE).types \
	$(DOC_OVERRIDES)

DOC_STAMPS = \
	scan-build.stamp \
	template-build.stamp \
	sgml-build.stamp \
	html-build.stamp \
	$(srcdir)/template.stamp \
	$(srcdir)/sgml.stamp \
	$(srcdir)/html.stamp

SCANOBJ_FILES = \
	$(DOC_MODULE).args \
	$(DOC_MODULE).hierarchy \
	$(DOC_MODULE).signals \
	$(DOC_MODULE).prerequisites \
	$(DOC_MODULE).interfaces

if ENABLE_GTK_DOC
# XXX: Uncomment following line to rebuild docs automatically
#all-local: docs

docs: html-build.stamp

#### scan ####

scan-build.stamp: $(HFILE_GLOB)
	@echo '*** Scanning header files ***'
	if test "x$(TYPES_INCLUDE)" != x; then \
	    echo "$(TYPES_INCLUDE)"; \
	    IGNORE_HFILES="$(IGNORE_HFILES)" $(top_srcdir)/devel-docs/extract-types.py $(HFILE_GLOB); \
	fi >$(srcdir)/$(DOC_MODULE).types
	if test -s $(srcdir)/$(DOC_MODULE).types; then \
	    CC="$(GTKDOC_CC)" LD="$(GTKDOC_LD)" CFLAGS="$(GTKDOC_CFLAGS)" LDFLAGS="$(GTKDOC_LIBS)" gtkdoc-scangobj $(SCANOBJ_OPTIONS) --module=$(DOC_MODULE) --output-dir=$(srcdir); \
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

template-build.stamp: $(DOC_MODULE)-decl.txt $(SCANOBJ_FILES) $(DOC_MODULE)-sections.txt $(DOC_OVERRIDES)
	@echo '*** Rebuilding template files ***'
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE) --output-dir=$(srcdir)/template
	if test "x$(REMOVE_SECTION_TITLES)" = "xyes"; then \
		for i in $(srcdir)/template/*.sgml; do \
			sed '2s/.*//' "$$i" >$(DOC_MODULE).rstmpl; \
			if diff "$$i" $(DOC_MODULE).rstmpl >/dev/null 2>&1; then :; else \
				cat $(DOC_MODULE).rstmpl >"$$i"; \
			fi; \
		done; \
		rm -f $(DOC_MODULE).rstmpl; \
	fi; \
	touch template-build.stamp

template.stamp: template-build.stamp
	@true

#### sgml ####

sgml-build.stamp: template.stamp $(CFILE_GLOB) $(srcdir)/template/*.sgml
	@echo '*** Building SGML ***'
	cd $(srcdir) && \
	gtkdoc-mkdb --module=$(DOC_MODULE) --tmpl-dir=$(srcdir)/template --source-dir=$(DOC_SOURCE_DIR) --sgml-mode --output-format=xml $(MKDB_OPTIONS)
	touch sgml-build.stamp

sgml.stamp: sgml-build.stamp
	@true

#### html ####

html-build.stamp: sgml.stamp $(DOC_MAIN_SGML_FILE) $(content_files)
	@echo '*** Building HTML ***'
	rm -rf $(srcdir)/html
	mkdir $(srcdir)/html
	cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
	test "x$(HTML_IMAGES)" = "x" || ( cd $(srcdir) && cp $(HTML_IMAGES) html )
	@echo '-- Fixing Crossreferences' 
	cd $(srcdir) && gtkdoc-fixxref --module-dir=html --html-dir=$(HTML_DIR) $(FIXXREF_OPTIONS)
	touch html-build.stamp
endif

##############

clean-local:
	rm -f *~ *.bak $(SCANOBJ_FILES) *-unused.txt $(DOC_STAMPS)

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf xml html $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt $(DOC_MODULE).types $(DOC_MODULE)-sections.txt

install-data-local:
	$(mkdir_p) $(DESTDIR)$(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*.html`; \
	if test "$$installfiles" = '$(srcdir)/html/*.html'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \
	fi)
	(installfiles=`echo $(srcdir)/html/*.png`; \
	if test "$$installfiles" != '$(srcdir)/html/*.png'; then \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	fi)

uninstall-local:
	(installfiles=`cd $(srcdir)/html >/dev/null && echo *.html`; \
	if test "$$installfiles" = '*.html'; \
	then echo '-- Nothing to uninstall' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Removing '$$i ; \
	    rm -f $(DESTDIR)$(TARGET_DIR)/$$i; \
	  done; \
	  echo '-- Removing index.sgml' ; \
	  rm -f $(DESTDIR)$(TARGET_DIR)/index.sgml; \
	fi)
	(installfiles=`cd $(srcdir)/html >/dev/null && echo *.png`; \
	if test "$$installfiles" != '*.png'; then \
	  for i in $$installfiles; do \
	    echo '-- Removing '$$i ; \
	    rm -f $(DESTDIR)$(TARGET_DIR)/$$i; \
	  done; \
	fi)

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
	-cp $(srcdir)/html/index.sgml $(distdir)/html
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html
	-cp $(srcdir)/html/*.png $(distdir)/html

.PHONY : dist-hook-local

