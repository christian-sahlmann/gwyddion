# @(#) $Id$

processdir = $(libexecdir)/@PACKAGE@/plugins/process

perl_plugins = invert_perl.pl
perl_rgis = invert_perl.pl.rgi
if HAVE_PERL
inst_perl_plugins = $(perl_plugins)
inst_perl_rgis = $(perl_rgis)
endif

python_plugins = invert_python.py
python_rgis = invert_python.py.rgi
if HAVE_PYTHON
inst_python_plugins = $(python_plugins)
inst_python_rgis = $(python_rgis)
endif

ruby_plugins = invert_ruby.rb invert_narray.rb
ruby_rgis = invert_ruby.rb.rgi
if HAVE_RUBY
inst_ruby_plugins = $(ruby_plugins)
inst_ruby_rgis = $(ruby_rgis)
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

pascal_rgis = invert_pascal.rgi
if HAVE_PASCAL
pascal_plugins = invert_pascal
inst_pascal_rgis = $(pascal_rgis)
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

process_DATA = \
	$(inst_pascal_rgis) \
	$(inst_perl_rgis) \
	$(inst_python_rgis) \
	$(inst_ruby_rgis) \
	invert_cpp.rgi \
	yellow.sh.rgi

all_rgis = \
	$(pascal_rgis) \
	$(perl_rgis) \
	$(python_rgis) \
	$(ruby_rgis) \
	invert_cpp.rgi \
	yellow.sh.rgi

# Prevent the silly automake from requiring invert_pascal.c for make dist
invert_pascal_SOURCES =

# Don't care about GwyddionDump, fpc seems to be clever enough to compile it
# automagically.
invert_pascal: invert_pascal.pas GwyddionDump.pas
	$(PASCAL) $(PASCAL_FLAGS) -oinvert_pascal invert_pascal.pas

noinst_HEADERS = \
	dump.hh \
	plugin-helper.hh

EXTRA_DIST = README $(script_plugins) $(pascal_plugins_sources) $(all_rgis)
INCLUDES = -I$(top_srcdir)
AM_CPPFLAGS = -DDEBUG -DG_LOG_DOMAIN=\"Plug-in\"
PASCAL_FLAGS = -Mdelphi -l- -Sh -WC -O2 -Xc -XX


clean-local:
	rm -f core.* *~ *.ppu
