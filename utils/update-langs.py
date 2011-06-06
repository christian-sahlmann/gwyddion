#!/usr/bin/python
# Update various lists of available localizations.
# The primary source is table languages in this file.
import re, os, sys

files_to_update = (
    'po/LINGUAS',
    'app/mac_integration.c',
    'data/gwyddion.nsit.in',
)

class Language:
    def __init__(self, key, isofull, name):
        self.key = key
        self.isofull = isofull
        self.name = name

    def __getitem__(self, key): return self.__dict__[key]
    def __contains__(self, key): return key in self.__dict__
    def __lt__(self, other): return self.key < other.key
    def __le__(self, other): return self.key <= other.key
    def __gt__(self, other): return self.key > other.key
    def __ge__(self, other): return self.key >= other.key

# The default language (English) must come first as it is treated specially
languages = (
    Language('en', 'en_US.UTF-8', 'English'),
    Language('cs', 'cs_CZ.UTF-8', 'Czech'),
    Language('de', 'de_DE.UTF-8', 'German'),
    Language('fr', 'fr_FR.UTF-8', 'French'),
    Language('it', 'it_IT.UTF-8', 'Italian'),
    Language('ru', 'ru_RU.UTF-8', 'Russian'),
    Language('es', 'es_ES.UTF-8', 'Spanish'),
)

class Template:
    def __init__(self, format, default=0, separator='\n'):
        self.format = format
        self.default = default
        self.separator = separator

    def fill(self, lang):
        return self.format % lang

    def fill_default(self, lang):
        if self.default == None:
            return ''
        if self.default == 0:
            return self.format % lang
        return self.default % lang

    def fill_all(self, langs):
        all = []
        for i, l in enumerate(langs):
            l.i12 = 12*(i + 1)
            if not i:
                if self.default != None:
                    all.append(self.fill_default(l))
            else:
                all.append(self.fill(l))
        return self.separator.join(all)

templates = {
    'LINGUAS': Template('%(key)s', None, ' '),
    'OS X': Template('        { "%(isofull)s", "%(key)s" },'),
    'NSIS-MENU': Template('    !insertmacro GWY_LOCALE_CHOOSER "%(name)s" "%(isofull)s" %(i12)uu'),
    'NSIS-MO': Template('    GwyExpandFiles "share\\locale\\%(key)s\\LC_MESSAGES\*.mo"'),
}

signature = (r'(?P<open>@@@ GENERATED LANG (?P<name>[A-Z_ -]+) BEGIN @@@[^\n]*$)'
             r'(?P<body>.*?)'
             r'(?P<close>^[^\n]*@@@ GENERATED LANG (?P=name) END @@@)')
sig_re = re.compile(signature, re.S | re.M)

def fill_template(m):
    name = m.group('name')
    if name not in templates:
        sys.stderr.write('Unknown LANG template %s\n' % name)
        return m.group()

    repl = templates[name].fill_all(languages)
    eol = ''
    if repl:
        eol = '\n'
    return m.group('open') + '\n' + repl + eol + m.group('close')

def process_file(filename):
    fh = file(filename, 'U')
    oldcontent = fh.read()
    newlines = fh.newlines
    fh.close()
    assert type(newlines) is str
    newcontent = sig_re.sub(fill_template, oldcontent)
    # Don't waste time running diff in the trivial case
    if oldcontent == newcontent:
        return

    if newlines != '\n':
        newcontent = newcontent.replace('\n', newlines)
    xgen = '%s.xgen' % filename
    file(xgen, 'wb').write(newcontent)
    # FIXME: status interpretation is system-dependent
    status = os.system('diff -u %s %s' % (filename, xgen)) >> 8
    if status == 1:
        sys.stderr.write('%s: Updating %s\n' % (sys.argv[0], filename))
        file(filename, 'w').write(newcontent)
    elif status > 1:
        sys.stderr.write('%s: Diff failed for %s\n' % (sys.argv[0], filename))
    os.unlink(xgen)

for filename in files_to_update:
    process_file(filename)

# vim: sw=4 ts=4 et:
