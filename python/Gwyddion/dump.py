#!/usr/bin/python

"""Gwyddion plug-in proxy dump dumb file format handling."""

# @(#) $Id$
# Written by Yeti <yeti@gwyddion.net>.
# Public domain.

import array as _array
import re as _re
import types as _types

def _dmove(d1, k1, d2, k2, typ=None):
    try:
        v = d1[k1]
    except KeyError:
        return
    if typ:
        d2[k2] = typ(v)
    else:
        d2[k2] = v
    del d1[k1]

def _read_dfield(fh, data, base):
    c = fh.read(1)
    if not c:
        return False
    if c != '[':
        # Python has no ungetc, seek one byte back
        fh.seek(-1, 1)
        return False
    dfield = {}
    _dmove(data, base + '/xres', dfield, 'xres', int)
    _dmove(data, base + '/yres', dfield, 'yres', int)
    _dmove(data, base + '/xreal', dfield, 'xreal', float)
    _dmove(data, base + '/yreal', dfield, 'yreal', float)
    _dmove(data, base + '/unit-xy', dfield, 'unit-xy')
    _dmove(data, base + '/unit-z', dfield, 'unit-z')
    a = _array.array('d')
    a.fromfile(fh, dfield['xres']*dfield['yres'])
    c = fh.readline()
    assert c == ']]\n'
    dfield['data'] = a
    data[base] = dfield
    return True

def read(filename):
    """Read a Gwyddion plug-in proxy dump file.

    The file is returned as a dictionary of dump key, value pairs.

    Data fields are packed into dictionaries with following keys
    (not all has to be present):
    `xres', x-resolution (number of samples),
    `yres', y-resolution (number of samples),
    `xreal', real x size (in base SI units),
    `yreal', real y size (in base SI units),
    `unit-xy', lateral units (base SI, like `m'),
    `unit-z', value units (base SI, like `m' or `A'),
    `data', the data field data itself (array of floats).

    The `data' member is a raw array of floats (please see array module
    documentation).

    Exceptions, caused by fatal errors, are not handled -- it is up to
    caller to eventually handle them."""

    line_re = _re.compile(r'^(?P<key>[^=]+)=(?P<val>.*)\n')
    field_re = _re.compile(r'^(?P<key>[^=]+)=\[\n')
    fh = file(filename, 'rb')
    data = {}
    while True:
        line = fh.readline()
        if not line: break
        m = field_re.match(line)
        if m and _read_dfield(fh, data, m.group('key')):
            continue
        m = line_re.match(line)
        if m:
            data[m.group('key')] = m.group('val')
            continue
        raise 'Can\'t understand input'
    fh.close()
    return data

def _dwrite(fh, dfield, base, key, fmt):
    if dfield.has_key(key):
        fh.write(('%s/%s=' + fmt + '\n') % (base, key, dfield[key]))

def _write_dfield(fh, dfield, base):
    _dwrite(fh, dfield, base, 'xres', '%d')
    _dwrite(fh, dfield, base, 'yres', '%d')
    _dwrite(fh, dfield, base, 'xreal', '%g')
    _dwrite(fh, dfield, base, 'yreal', '%g')
    _dwrite(fh, dfield, base, 'unit-xy', '%s')
    _dwrite(fh, dfield, base, 'unit-z', '%s')
    fh.write('%s=[\n[' % base)
    dfield['data'].tofile(fh)
    fh.write(']]\n')

def write(data, filename):
    """Write a Gwyddion plug-in proxy dump file.

    The dictionary to write is expected to follow the same conventions as
    those returned by read(), please see its description for more.

    Exceptions, caused by fatal errors, are not handled -- it is up to
    caller to eventually handle them."""
    fh = file(filename, 'wb')
    for k, v in data.items():
        if type(v) == _types.DictType:
            continue
        fh.write('%s=%s\n' % (k, v))
    for k, v in data.items():
        if type(v) != _types.DictType:
            continue
        _write_dfield(fh, v, k)
    fh.close()

