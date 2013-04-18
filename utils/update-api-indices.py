#!/usr/bin/python
# Update various lists of available localizations.
# The primary source is table languages in this file.
import re, os, sys, glob

subdirs = (
    'libgwyddion',
    'libgwyprocess',
    'libgwydraw',
    'libgwydgets',
    'libgwymodule',
    'libgwyapp',
)

bmark = r'  <!-- API INDICES BEGIN -->\n'
emark = r'  <!-- API INDICES END -->\n'

template = '''\
  <index id="api-index-%s"%s>
    <title>Index of %s</title>
    <xi:include href="xml/api-index-%s.xml"><xi:fallback /></xi:include>
  </index>
'''

def keyfunc(name):
    if name == 'full':
        return -2.0
    if name == 'deprecated':
        return -1.0
    if not re.match('^\d+\.\d+', name):
        return 0.0
    name = name.split('.')
    return float(name[0]) + 0.001*float(name[1])

for subdir in subdirs:
    docsfile = os.path.join('devel-docs', subdir, subdir + '-docs.sgml')
    if not os.access(docsfile, os.R_OK):
        continue

    indices = glob.glob(os.path.join('devel-docs', subdir, 'xml', 'api-index-*.xml'))
    indices = [re.sub('^.*?/api-index-([^/]+)\.xml$', r'\1', x) for x in indices]
    out = []
    for ind in sorted(indices, key=keyfunc):
        if ind == 'full':
            xmlid = ind
            name = 'all symbols'
            role = ''
        elif ind == 'deprecated':
            xmlid = ind
            name = 'deprecated symbols'
            role = ' role="%s"' % ind
        else:
            xmlid = re.sub(r'[^0-9a-z]+', '-', ind)
            name = 'new symbols in %s' % ind
            role = ' role="%s"' % ind
        out.append(template % (xmlid, role, name, ind))

    text = file(docsfile).read()
    text = re.sub(r'(?s)' + bmark + r'.*?' + emark,
                  bmark + ''.join(out) + emark,
                  text)
    file(docsfile, 'w').write(text)

# vim: sw=4 ts=4 et:
