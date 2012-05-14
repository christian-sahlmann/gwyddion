#!/usr/bin/python
# vim: sw=4 ts=4 et :
# Written by Yeti <yeti@gwyddion.net>. Public domain.
import sys, os, re

# The global mapping between footnote ids (as they appear in the guide) and
# their texts.  This is used to compactify footnotes with an identical text.
footnotes = {}

def format_userguide(module, body):
    DESCRIPTION, EXTENSIONS, SUPPORTS, NOTES = range(4)
    parsesupport = re.compile(r'(?P<name>\w+)'
                              r'(?::(?P<alias>\w+))?'
                              r'(?:\[(?P<note>\w+)\])?').match
    parsenotes = re.compile(r'(?ms)\[(?P<note>\w+)\]\s*(?P<text>[^[]*)')
    entry = '  <entry>%s</entry>'

    # Return the local mapping for a footnote (i.e. between [1] in the comment
    # in source code and the text that should go to the user guide).
    def register_note(footnotes, note):
        key, text = note.group('note'), note.group('text').strip()
        for fnkey, entry in footnotes.items():
            fntext, label = entry
            if text == fntext:
                fmttext = '<footnoteref linkend="%s" label="%s"/>' \
                          % (fnkey, label)
                return key, fmttext
        fnkey = 'table-file-formats-fn%d' % (len(footnotes) + 1)
        label = chr(ord('a') + len(footnotes))
        fmttext = '<footnote id="%s" label="%s"><para>%s</para></footnote>' \
                  % (fnkey, label, text)
        footnotes[fnkey] = (text, label)
        return key, fmttext

    def fmtsup(supported, noterefs, what):
        yes = 'Yes'
        for x in supported:
            name = x['name']
            if name in what:
                if x['alias']:
                    yes = x['alias']
                if x['note']:
                    yes += noterefs[x['note']]
                return yes
        return 'No'

    module = re.sub(r'\.c$', '', module)
    out = ['<row>']
    lines = body.split('\n')[:NOTES]
    noterefs = {}
    for note in parsenotes.finditer('\n'.join(body.split('\n')[NOTES:])):
        key, text = register_note(footnotes, note)
        noterefs[key] = text
    out.append(entry % lines[DESCRIPTION])
    out.append(entry % ', '.join(lines[EXTENSIONS].split()))
    out.append(entry % module)
    supinfo = [parsesupport(x).groupdict() for x in lines[SUPPORTS].split()]
    out.append(entry % fmtsup(supinfo, noterefs, ('Read',)))
    out.append(entry % fmtsup(supinfo, noterefs, ('Save', 'Export')))
    out.append(entry % fmtsup(supinfo, noterefs, ('SPS',)))
    out.append('</row>')
    for i, x in enumerate(out):
        out[i] = '    ' + x
    return '\n'.join(out)

mclasses = {
    'FREEDESKTOP': {
        'comment': '<!-- %s -->',
        'header': '''<?xml version="1.0"?>
<mime-info xmlns='http://www.freedesktop.org/standards/shared-mime-info'>''',
        'footer': '</mime-info>'
    },
    'FILE': {
        'comment': '# %s',
    },
    'USERGUIDE': {
        'comment': '    <!-- %s -->',
        'header': '''<?xml version='1.0' encoding='utf-8'?>
<!DOCTYPE book PUBLIC '-//OASIS//DTD DocBook XML V4.5//EN'
               'http://www.oasis-open.org/docbook/xml/4.5/docbookx.dtd'>
<informaltable frame='none' id='table-file-formats'>
  <indexterm>
    <primary>file</primary>
    <primary>supported formats</primary>
  </indexterm>
  <tgroup cols='6' align='left'>
    <?dblatex XXXccc?>
    <thead>
      <row>
        <entry>File Format</entry>
        <entry>Extensions</entry>
        <entry>Module</entry>
        <entry>Read</entry>
        <entry>Write</entry>
        <entry>SPS</entry>
      </row>
    </thead>
    <tbody>''',
        'footer': '''    </tbody>
  </tgroup>
</informaltable>''',
        'formatter': format_userguide,
     },
}

try:
    magic = mclasses[sys.argv[1]]
except KeyError:
    sys.stderr.write('Unknown magic class %s\n' % sys.argv[1])
    sys.exit(1)
except IndexError:
    print 'Usage: %s CLASS [FILES...]' % sys.argv[0]
    print 'Available classes: %s' % ' '.join(mclasses.keys())
    sys.exit(0)

magic_block_re = re.compile(r'(?m)^/\*\*\n \* '
                            + r'\[FILE-MAGIC-' + sys.argv[1] + r'\]\n'
                            + r'(?P<body>(?: \* .*\n)+) \*\*/$')
output = []

if magic.has_key('header'):
    output.append(magic['header'])

# Keep the `GENERATED' string split to prevent match here
output.append(magic['comment'] % ('This is a ' + 'GENERATED' + ' file.'))

for filename in sys.argv[2:]:
    base = os.path.basename(filename)
    try:
        # output From module... comment only if something is found
        comment = magic['comment'] % ('From module ' + base)
        for m in magic_block_re.finditer(file(filename).read()):
            if comment:
                output.append(comment)
                comment = None
            body = re.sub(r'(?m)^ \* ', '', m.group('body'))
            if 'formatter' in magic:
                body = magic['formatter'](base, body)
            output.append(body)
        # and when nothing is found, note it
        if comment:
            comment = magic['comment'] % ('Module %s contains no magic.' % base)
            output.append(comment)
    except OSError:
        sys.stderr.write('Cannot read %s\n' % filename)
        sys.exit(1)

if magic.has_key('footer'):
    output.append(magic['footer'])

print '\n'.join(output)
