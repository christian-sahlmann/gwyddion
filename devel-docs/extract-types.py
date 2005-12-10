#!/usr/bin/python
import sys, re, os

r = re.compile(r'#define\s+(?P<type>[A-Z0-9_]+)_GET_(?:CLASS|IFACE)\b')

ignore_files = os.environ.get('IGNORE_HFILES').split()
ignore_files = dict([(x, 1) for x in ignore_files])

for f in sys.argv[1:]:
    if os.path.basename(f) in ignore_files:
        continue
    fh = file(f)
    for line in fh:
        m = r.match(line)
        if not m:
            continue
        print m.group('type').lower() + '_get_type'
        break
    fh.close()
