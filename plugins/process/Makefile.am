# @(#) $Id$

processdir = $(gwyplugindir)/process

perl_plugins = invert_perl.pl
if HAVE_PERL
inst_perl_plugins = $(perl_plugins)
endif

python_plugins = invert_python.py
if HAVE_PYTHON
inst_python_plugins = $(python_plugins)
endif

ruby_plugins = invert_ruby.rb invert_narray.rb
if HAVE_RUBY
inst_ruby_plugins = $(ruby_plugins)
endif

script_plugins = \
	$(perl_plugins) \
	$(python_plugins) \
	$(ruby_plugins) \
	yellow.sh

process_SCRIPTS = \
	$(inst_perl_plugins) \
	$(inst_python_plugins) \
	$(inst_ruby_plugins) \
	yellow.sh

if HAVE_PASCAL
pascal_plugins = \
	invert_pascal
else
pascal_plugins =
endif

pascal_plugins_sources = \
	GwyddionDump.pas \
	invert_pascal.pas

process_PROGRAMS = \
	invert_cpp \
	$(pascal_plugins)

invert_cpp_SOURCES = \
	dump.cc \
	invert_cpp.cc

# Prevent the silly automake from requiring invert_pascal.c for make dist
invert_pascal_SOURCES =

# Don't care about GwyddionDump, fpc seems to be clever enough to compile it
# automagically.
invert_pascal: invert_pascal.pas GwyddionDump.pas
	$(PASCAL) $(PASCAL_FLAGS) -oinvert_pascal invert_pascal.pas

noinst_HEADERS = \
	dump.hh \
	plugin-helper.hh

EXTRA_DIST = README $(script_plugins) $(pascal_plugins_sources)
INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Plug-in\"
PASCAL_FLAGS = -Mdelphi -l- -Sh -WC -O2 -Xc -XX


clean-local:
	rm -f core.* *~ *.ppu
