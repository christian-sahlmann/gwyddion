#!/usr/bin/python
# vim: sw=4 ts=4 et :
# Written by Yeti <yeti@gwyddion.net>. Public domain.
import sys, os, re

def format_userguide(module, body):
    def fmtsup(supported, what):
        supported = [re.sub(r'\W', '', x) for x in supported]
        for x in what:
            if x in supported:
                return 'Yes'
        return 'No'

    module = re.sub(r'\.c$', '', module)
    lines = ['<row>']
    input = body.split('\n')
    lines.append('  <entry>%s</entry>' % input[0])
    lines.append('  <entry>%s</entry>' % ', '.join(input[1].split()))
    lines.append('  <entry>%s module</entry>' % module)
    supports = input[2].split()
    lines.append('  <entry>%s</entry>' % fmtsup(supports, ('Read',)))
    lines.append('  <entry>%s</entry>' % fmtsup(supports, ('Save', 'Export')))
    lines.append('  <entry>%s</entry>' % fmtsup(supports, ('SPS',)))
    lines.append('</row>')
    for i, x in enumerate(lines):
        lines[i] = '    ' + x
    return '\n'.join(lines)

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
        <entry>Supported By</entry>
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
