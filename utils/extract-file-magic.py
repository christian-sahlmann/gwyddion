#!/usr/bin/python
import sys, os, re

mclasses = {
    'FREEDESKTOP': {
        'comment': '<!-- %s -->',
        'header': '''<?xml version="1.0"?>
<mime-info xmlns='http://www.freedesktop.org/standards/shared-mime-info'>''',
        'footer': '</mime-info>'
    },
    'FILE': {
        'comment': '# %s',
    }
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

for filename in sys.argv[2:]:
    base = os.path.basename(filename)
    try:
        comment = magic['comment'] % ('From module ' + base)
        for m in magic_block_re.finditer(file(filename).read()):
            if comment:
                output.append(comment)
                comment = None
            output.append(re.sub(r'(?m)^ \* ', '', m.group('body')))
    except OSError:
        sys.stderr.write('Cannot read %s\n' % filename)
        sys.exit(1)

if magic.has_key('footer'):
    output.append(magic['footer'])

print '\n'.join(output)
