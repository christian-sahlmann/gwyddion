# @(#) $Id$

libgwyddion = $(top_builddir)/libgwyddion/libgwyddion.la
libgwymodule = $(top_builddir)/libgwymodule/libgwymodule.la
libgwyprocess = $(top_builddir)/libprocess/libgwyprocess.la
libgwydraw = $(top_builddir)/libdraw/libgwydraw.la
libgwydgets = $(top_builddir)/libgwydgets/libgwydgets.la
libgwyapp = $(top_builddir)/app/libgwyapp.la
moduledir = $(top_srcdir)/modules

EXTRA_DIST = \
	modules-web.xsl \
	update-msvc.py \
	update-potfiles.sh \
	vim-syn-gen.py

if MAINTAINER_MODE
all-local: modules.xml module-list.html module-list-nocss.html

noinst_PROGRAMS = \
	dump-modules

dump_modules_SOURCES = \
	dump-modules.c

dump_modules_LDADD = @GTKGLEXT_LIBS@ \
	$(libgwyapp) \
	$(libgwymodule) \
	$(libgwydgets) \
	$(libgwydraw) \
	$(libgwyprocess) \
	$(libgwyddion)

INCLUDES = -I$(top_srcdir)

modules.xml: dump-modules $(moduledir)/*.c $(moduledir)/*/*.c
	./dump-modules >modules.xml

module-list.html: modules.xml modules-web.xsl
	$(XSLTPROC) modules-web.xsl modules.xml >module-list.html

module-list-nocss.html: modules.xml modules-web-nocss.xsl
	$(XSLTPROC) modules-web-nocss.xsl modules.xml >module-list-nocss.html
endif

clean-local:
	rm -f core.* *~
	rm -f modules.xml module-list.html module-list-nocss.html
