#!/usr/bin/python
# Remap local documentation cross-links to on-line URLs and remove redundant
# document name from self-links.
import re, sys, os.path

# Base locations of on-line documentation:
www = {
    'cairo':         'http://www.cairographics.org/manual',
    'dbus':          'http://dbus.freedesktop.org/doc',
    'gimp':          'http://developer.gimp.org/api/2.0',
    'gnome':         'http://developer.gnome.org/doc/API/2.0',
    'gstreamer':     'http://gstreamer.freedesktop.org/data/doc/gstreamer/stable',
    'gtkglext':      'http://gtkglext.sourceforge.net/reference',
    'gtksourceview': 'http://gtksourceview.sourceforge.net/docs',
    'rsvg':          'http://librsvg.sourceforge.net/docs/html',
}

# Directories corresponding to various libraries:
libdirs = {
    'at-spi':            ('gnome',         'at-spi'),
    'atk':               ('gnome',         'atk'),
    'bonobo-activation': ('gnome',         'bonobo-activation'),
    'cairo':             ('cairo',         ''),
    'dbus-glib':         ('dbus',          'dbus-glib'),
    'gail':              ('gnome',         'gail'),
    'gconf':             ('gnome',         'gconf'),
    'gdk':               ('gnome',         'gdk'),
    'gdk-pixbuf':        ('gnome',         'gdk-pixbuf'),
    'glib':              ('gnome',         'glib'),
    'gnome-vfs-2.0':     ('gnome',         'gnome-vfs-2.0'),
    'gobject':           ('gnome',         'gobject'),
    'gsf':               ('gnome',         'libgsf'),
    'gstreamer':         ('gstreamer',     'gstreamer/html'),
    'gstreamer-libs':    ('gstreamer',     'gstreamer-libs/html'),
    'gtk':               ('gnome',         'gtk'),
    'gtkglext':          ('gtkglext',      'gtkglext'),
    'gtksourceview':     ('gtksourceview', ''),
    'libbonobo':         ('gnome',         'libbonobo'),
    'libbonoboui':       ('gnome',         'libbonoboui'),
    'libgimp':           ('gimp',          'libgimp'),
    'libgimpbase':       ('gimp',          'libgimpbase'),
    'libgimpcolor':      ('gimp',          'libgimpcolor'),
    'libgimpmath':       ('gimp',          'libgimpmath'),
    'libgimpmodule':     ('gimp',          'libgimpmodule'),
    'libgimpthumb':      ('gimp',          'libgimpthumb'),
    'libgimpwidgets':    ('gimp',          'libgimpwidgets'),
    'libglade':          ('gnome',         'libglade'),
    'libgnome':          ('gnome',         'libgnome'),
    'libgnomecanvas':    ('gnome',         'libgnomecanvas'),
    'libgnomeui':        ('gnome',         'libgnomeui'),
    'libsoup':           ('gnome',         'libsoup'),
    'ORBit2':            ('gnome',         'ORBit'),
    'pango':             ('gnome',         'pango'),
    'rsvg':              ('rsvg',          ''),
    'vte':               ('gnome',         'vte'),
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

