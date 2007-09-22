#!/usr/bin/python
# Update source code to match stock icons listed in pixmaps/Makefile.am
import re, os, sys, popen2

base = 'libgwydgets/gwystock'
makefile = 'pixmaps/Makefile.am'

def read_images(f):
    """Read image list from Makefile"""
    images = {}
    in_data = False
    for line in file(f):
        line = line.strip()
        if not line:
            in_data = False
            continue
        if line.startswith('pixmapdata_DATA ='):
            in_data = True
            continue
        if not in_data:
            continue
        m = re.match(r'^gwy_(?P<name>[a-z0-9_]+)-(?P<size>[0-9]+)\.png\b', line)
        if not m:
            continue
        images.setdefault(m.group('name'), [])
        images[m.group('name')].append(int(m.group('size')))
    assert not in_data
    return images

def read_since(f):
    """Read `Since' notes from Makefile"""
    sinces = {}
    for line in file(f):
        m = re.match(r'^\s*#\s+Since\s+(?P<version>[0-9.]+):\s*(?P<list>[-a-z0-9_. ]+)', line)
        if not m:
            continue
        for i in m.group('list').split():
            assert i not in sinces
            sinces[i] = m.group('version')
    return sinces

def replace_file(f, replacement):
    oldcontent = file(f).read()
    newcontent = re.sub(r'(?s)'
                        r'(/\* @@@ GENERATED STOCK LIST BEGIN @@@ \*/\n)'
                        r'(.*)'
                        r'(/\* @@@ GENERATED STOCK LIST END @@@ \*/\n)',
                        r'\1' + replacement + r'\3',
                        oldcontent)
    # Don't waste time running diff in the trivial case
    if oldcontent == newcontent:
        return

    xgen = '%s.xgen' % f
    file(xgen, 'w').write(newcontent)
    # FIXME: status interpretation is system-dependent
    status = os.system('diff -u %s %s' % (f, xgen)) >> 8
    if status == 1:
        sys.stderr.write('%s: Updating %s\n' % (sys.argv[0], f))
        file(f, 'w').write(newcontent)
    elif status > 1:
        sys.stderr.write('%s: Diff failed for %s\n' % (sys.argv[0], f))
    os.unlink(xgen)

def update_macros(images):
    """Update header file with stock icon macro definitions."""

    # Format #defines
    hfile = base + '.h'
    defines = ['#define GWY_STOCK_%s "gwy_%s"\n' % (x.upper(), x)
            for x in images.keys()]
    defines.sort()
    i, o = popen2.popen2('./utils/align-declarations.py')
    o.write(''.join(defines))
    o.close()
    defines = i.read()
    i.close()
    replace_file(hfile, defines)

def update_documentation(images, sinces):
    """Update `documentation' in C source to list all stock icons."""

    # Format documentation entries
    # FIXME: Sometimes the image paths seem to need to `../' prepended becuase
    # content files are XIncluded from xml/ subdirectory, therefore straight
    # paths get xml/ prepended to be relative to the driver file.  Sometimes
    # this causes extra `../' in the paths.  Not sure what the `right' thing
    # is.
    cfile = base + '.c'
    template = ('/**\n'
                ' * GWY_STOCK_%s:\n'
                ' *\n'
                ' * The "%s" stock icon.\n'
                ' * <inlinegraphic fileref="gwy_%s-%d.png" format="PNG"/>\n'
                '%s'
                ' **/\n'
                '\n')
    docs = []
    for k, v in images.items():
        words = re.sub(r'_1_1', '_1:1', k).split('_')
        for i in range(len(words)):
            # Heuristics: words without wovels are abbreviations
            if not re.search(r'[aeiouy]', words[i]):
                words[i] = words[i].upper()
            else:
                words[i] = words[i].title()
        human_name = '-'.join(words)

        if 24 in v:
            size = 24
        else:
            size = max(v)

        if sinces.has_key(k):
            s = ' *\n * Since: %s\n' % sinces[k]
            del sinces[k]
        else:
            s = ''

        docs.append(template % (k.upper(), human_name, k, size, s))
    docs.sort()
    docs = ''.join(docs)
    replace_file(cfile, docs)

imgs = read_images(makefile)
sincs = read_since(makefile)
update_macros(imgs)
update_documentation(imgs, sincs)
# Check for unused since declarations, they are typos
assert not sincs
