#!/usr/bin/python
# @(#) $Id$
import sys, os, array, re, types

"""This is an example Python plug-in.

It just divides all values by 2.
"""

valid_arguments = ('register', 'run')

plugin_info = """\
divide_by_2
/_Divide by 2 (Python)
noninteractive with_defaults\
"""

line_re = re.compile(r'(?P<key>[^=]+)=(?P<val>.*)\n')
field_re = re.compile(r'(?P<key>[^=]+)=\[\n')

stdout = os.fdopen(1, 'wb')
stderr = os.fdopen(2, 'wb')

def error(m):
    stderr.write('*** Error: %s\n' % m)
    sys.exit(1)

def dpop(d, k):
    v = d[k]
    del d[k]
    return v

def read_data(filename):
    fh = file(filename, 'rb')
    data = {}
    while True:
        line = fh.readline()
        if not line: break
        m = field_re.match(line)
        if m:
            c = fh.read(1)
            assert c == '['
            key = m.group('key')
            xres = int(dpop(data, key + '/xres'))
            yres = int(dpop(data, key + '/yres'))
            xreal = float(dpop(data, key + '/xreal'))
            yreal = float(dpop(data, key + '/yreal'))
            a = array.array('d')
            a.fromfile(fh, xres*yres)
            c = fh.readline()
            assert c == ']]\n'
            data[key] = {'xres': xres, 'yres': yres,
                         'xreal': xreal, 'yreal': yreal,
                         'data': a}
            continue
        m = line_re.match(line)
        if m:
            data[m.group('key')] = m.group('val')
            continue
        raise 'Can\'t understand input'
    return data

def print_data(data):
    for k, v in data.items():
        if type(v) == types.DictType: continue
        stdout.write('%s=%s\n' % (k, v))
    for k, v in data.items():
        if type(v) != types.DictType: continue
        stdout.write('%s/xres=%d\n' % (k, v['xres']))
        stdout.write('%s/yres=%d\n' % (k, v['yres']))
        stdout.write('%s/xreal=%g\n' % (k, v['xreal']))
        stdout.write('%s/yreal=%g\n' % (k, v['yreal']))
        stdout.write('%s=[\n[' % k)
        v['data'].tofile(stdout)
        stdout.write(']]\n')

args = sys.argv
args.pop(0)
if not args or args[0] not in valid_arguments:
    error("plug-in has to be called from Gwyddion plugin-proxy.")
what = args.pop(0)
if what == 'register':
    print plugin_info
elif what == 'run':
    run = args.pop(0)
    data = read_data(args.pop(0))
    data['/meta/A subliminal message'] = 'Python rulez!'
    df = data['/0/data']['data']
    stderr.write('len(df) = %d\n' % len(df))
    for i in range(len(df)):
        df[i] /= 2.0
    print_data(data)
sys.exit(0)
