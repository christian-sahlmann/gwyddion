# @(#) $Id$

Gwyddion_RUBY = \
	gwyddion/dump.rb

EXTRA_DIST = $(Gwyddion_RUBY)

if HAVE_RUBY
RUBY_TARGET_DIR = $(gwypluginlibdir)/ruby/gwyddion

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

