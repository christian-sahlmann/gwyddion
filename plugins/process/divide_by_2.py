#!/usr/bin/python
# @(#) $Id$
# A very simple Gwyddion plug-in example in Python.  Demonstrates reading,
# modifying and outputting the data.
# Written by Yeti <yeti@physics.muni.cz>.
# Public domain.
import sys, os, array, re, types

# Plug-in information.
# Format is similar to GwyProcessFuncInfo:
# - Name (just an unique identifier)
# - Menu path
# - Run modes (possible values: noninteractive with_defaults interactive modal)
plugin_info = """\
divide_by_2
/_Test/_Divide by 2 (Python)
noninteractive with_defaults\
"""

valid_arguments = 'register', 'run'

line_re = re.compile(r'^(?P<key>[^=]+)=(?P<val>.*)\n')
field_re = re.compile(r'^(?P<key>[^=]+)=\[\n')

stdout = os.fdopen(1, 'wb')
stderr = os.fdopen(2, 'wb')

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

def process_data(data):
    data['/meta/A subliminal message'] = 'Python rulez!'
    df = data['/0/data']['data']
    for i in range(len(df)):
        df[i] /= 2.0

args = sys.argv
args.pop(0)
if not args or args[0] not in valid_arguments:
    raise "Plug-in has to be called from Gwyddion plugin-proxy."

what = args.pop(0)
if what == 'register':
    print plugin_info
elif what == 'run':
    run = args.pop(0)
    filename = args.pop(0)
    data = read_data(filename)
    process_data(data)
    print_data(data)
sys.exit(0)
