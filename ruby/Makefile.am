# @(#) $Id$

if HAVE_RUBY
RUBY_TARGET_DIR = ${prefix}@RUBY_SITE_LIB@/gwyddion
EXTRA_DIST = $(Gwyddion_RUBY)

Gwyddion_RUBY = \
	gwyddion/dump.rb

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(RUBY_TARGET_DIR)
	for i in $(Gwyddion_RUBY); do \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(RUBY_TARGET_DIR); \
	done

uninstall-local:
	for i in $(Gwyddion_RUBY); do \
	    rm -f $(DESTDIR)$(RUBY_TARGET_DIR)/$$i; \
	done
	-rmdir $(DESTDIR)$(RUBY_TARGET_DIR)
endif

clean-local:
	rm -f *~ *.bak

