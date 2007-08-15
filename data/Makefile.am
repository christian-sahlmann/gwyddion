# @(#) $Id$

# Some combinations of the tools do not give these us automatically.
abs_top_srcdir = @abs_top_srcdir@
abs_srcdir = @abs_srcdir@

SUBDIRS = \
	glmaterials \
	gradients

pkgconfigdatadir = $(libdir)/pkgconfig
gwyddionappdir = $(datadir)/applications
gwyddionmimedir = $(datadir)/mime/packages

docsrc = $(top_builddir)/devel-docs
vim_syn_gen = $(abs_top_srcdir)/utils/vim-syn-gen.py
gwyddion_vim_cfg = $(abs_srcdir)/gwyddion-cfg.py

pkgconfigdata_DATA = gwyddion.pc
gwyddionapp_DATA = gwyddion.desktop
gwyddionmime_DATA = gwyddion.xml
man1_MANS = gwyddion.1

EXTRA_DIST = \
	config.h.win32.in \
	config.h.win32 \
	gwyconfig.h.win32 \
	gwyddion-devel.iss \
	gwyddion-devel.iss.in \
	gwyddion.iss \
	gwyddion.iss.in \
	gwyddion.pc.in \
	gwyddion.spec.in \
	gwyddion-cfg.py \
	gwyddion.vim \
	gwyddion.1 \
	makefile.msc \
	$(gwyddionapp_DATA) \
	$(gwyddionmime_DATA)

# gwyddion.vim generation
gwyddion_vim_sources = \
	$(docsrc)/libgwyapp/libgwyapp-decl.txt \
	$(docsrc)/libgwyddion/libgwyddion-decl.txt \
	$(docsrc)/libgwydgets/libgwydgets-decl.txt \
	$(docsrc)/libgwydraw/libgwydraw-decl.txt \
	$(docsrc)/libgwymodule/libgwymodule-decl.txt \
	$(docsrc)/libgwyprocess/libgwyprocess-decl.txt

file_magic_sources = $(top_srcdir)/modules/file/*.c

MAINTAINERCLEANFILES = \
	gwyddion.vim \
	gwyddion.xml

# FIXME: this breaks VPATH build
if MAINTAINER_MODE
gwyddion.vim: $(gwyddion_vim_sources) $(gwyddion_vim_cfg) $(vim_syn_gen)
	cd $(top_builddir); \
	$(PYTHON) $(vim_syn_gen) $(gwyddion_vim_cfg) >data/gwyddion.vim

# Desktop files
gwyddion.xml: $(top_srcdir)/utils/extract-file-magic.py $(file_magic_sources)
	$(PYTHON) $(top_srcdir)/utils/extract-file-magic.py \
	  FREEDESKTOP $(file_magic_sources) >gwyddion.xml
endif

# Desktop file installation
# FIXME: Maybe not according to current standards
if DESKTOP_FILE_UPDATE
install-data-hook:
	$(UPDATE_DESKTOP_DATABASE) $(DESTDIR)$(datadir)/applications
	$(UPDATE_MIME_DATABASE) $(DESTDIR)$(datadir)/mime

uninstall-hook:
	$(UPDATE_DESKTOP_DATABASE) $(DESTDIR)$(datadir)/applications
	$(UPDATE_MIME_DATABASE) $(DESTDIR)$(datadir)/mime
endif

