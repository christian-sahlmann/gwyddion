# @(#) $Id$

Gwyddion_RUBY = \
	gwyddion/dump.rb

EXTRA_DIST = $(Gwyddion_RUBY)

if HAVE_RUBY
rubylibdir = $(gwypluginlibdir)/ruby/gwyddion

rubylib_DATA = $(Gwyddion_RUBY)
endif

clean-local:
	rm -f *~ *.bak

