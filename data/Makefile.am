# @(#) $Id$

SUBDIRS = \
	gradients

pkgconfigdatadir = $(libdir)/pkgconfig
gwyddionappdir = $(datadir)/applications
gwyddionmimedir = $(datadir)/mime/packages

docsrc = $(top_srcdir)/devel-docs
vim_syn_gen = $(top_srcdir)/utils/vim-syn-gen.py
gwyddion_vim_cfg = $(top_srcdir)/data/gwyddion-cfg.py

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

MAINTAINERCLEANFILES = \
	gwyddion.vim

# FIXME: this breaks VPATH build
if MAINTAINER_MODE
gwyddion.vim: $(gwyddion_vim_sources) $(gwyddion_vim_cfg) $(vim_syn_gen)
	cd $(top_builddir); \
	$(PYTHON) utils/vim-syn-gen.py data/gwyddion-cfg.py >data/gwyddion.vim
endif

# Desktop files
if DESKTOP_FILE_UPDATE
install-data-hook:
	$(UPDATE_DESKTOP_DATABASE) $(datadir)/applications
	$(UPDATE_MIME_DATABASE) $(datadir)/mime

uninstall-hook:
	$(UPDATE_DESKTOP_DATABASE) $(datadir)/applications
	$(UPDATE_MIME_DATABASE) $(datadir)/mime
endif

