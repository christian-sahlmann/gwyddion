#!/usr/bin/python
# Remap local documentation cross-links to on-line URLs and remove redundant
# document name from self-links.
import re, sys, os.path

# Base locations of on-line documentation:
www = {
    'gtk':      'http://developer.gnome.org/doc/API/2.0',
    'gtkglext': 'http://gtkglext.sourceforge.net/reference',
    'cairo':    'http://www.cairographics.org/manual',
    'gimp':     'http://developer.gimp.org/api/2.0',
}

# Directories corresponding to various libraries:
libdirs = {
    'atk':            ('gtk',      'atk'),
    'cairo':          ('cairo',    ''),
    'gdk':            ('gtk',      'gdk'),
    'gdk-pixbuf':     ('gtk',      'gdk-pixbuf'),
    'glib':           ('gtk',      'glib'),
    'gobject':        ('gtk',      'gobject'),
    'gtk':            ('gtk',      'gtk'),
    'gtkglext':       ('gtkglext', 'gtkglext'),
    'libgimp':        ('gimp',     'libgimp'),
    'libgimpbase':    ('gimp',     'libgimpbase'),
    'libgimpcolor':   ('gimp',     'libgimpcolor'),
    'libgimpmath':    ('gimp',     'libgimpmath'),
    'libgimpmodule':  ('gimp',     'libgimpmodule'),
    'libgimpthumb':   ('gimp',     'libgimpthumb'),
    'libgimpwidgets': ('gimp',     'libgimpwidgets'),
    'pango':          ('gtk',      'pango'),
}

# Compose the two above
docmap = dict([(k, www[v[0]] + '/' + v[1]) for k, v in libdirs.items()])

unknowndoc = {}

def map1(m):
    d = m.group('dir')
    if not docmap.has_key(d):
        if not unknowndoc.has_key(d):
            sys.stderr.write(d + 'documentation location is unknown.\n')
            unknowndoc[d] = 'have seen this one'
        d = 'gtk'
    return m.group('a') + docmap[d] + m.group('file')

def process(text, self=None):
    # Remap local cross-links to on-line URLs
    text = re.sub(r'(?P<a><a[ \t\n]+href=")/.*?/'
                  r'(?P<dir>[^"/]+)(?P<file>/[^"/]+")',
                  map1, text, re.S)
    # Remove redundant document name from links to self
    if self:
        self = re.escape(self)
        text = re.sub(r'(<a[ \t\n]+href=")' + self + r'#([-A-Za-z0-9_:.]*")',
                      r'\1#\2', text, re.S)
        text = re.sub(r'(<a[ \t\n]+href=")' + self + r'(")',
                      r'\1#\2', text, re.S)
    return text

if len(sys.argv) > 1:
    for filename in sys.argv[1:]:
        text = file(filename).read()
        text = process(text, os.path.basename(filename))
        file(filename, 'w').write(text)
else:
    sys.stdout.write(process(sys.stdin.read()))

