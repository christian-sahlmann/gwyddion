# @(#) $Id$

Gwyddion_RUBY = \
	gwyddion/dump.rb

EXTRA_DIST = $(Gwyddion_RUBY)

rubylibdir = $(pkglibdir)/ruby/gwyddion

if HAVE_RUBY
rubylib_DATA = $(Gwyddion_RUBY)
endif

clean-local:
	rm -f *~ *.bak

