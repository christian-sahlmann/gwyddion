#!/usr/bin/env python
# @(#) $Id$
# A very simple Gwyddion plug-in example in Python.
# Written by Yeti <yeti@gwyddion.net>.  Public domain.
import sys, os
sys.path.append(os.path.join(os.environ['GWYPLUGINLIB'], 'python'))
import Gwyddion

# Plug-in information.
run_modes = 'noninteractive', 'with_defaults'

plugin_info = """\
invert_python
/_Test/Value Invert (Python)
""" + ' '.join(run_modes)

def register(args):
    print plugin_info

def run(args):
    run_mode = args.pop(0)
    assert run_mode in run_modes

    filename = args.pop(0)
    data = Gwyddion.dump.read(filename)
    dfield = data['/0/data']
    a = dfield['data']

    n = len(a)
    mirror = min(a) + max(a)
    for i in range(n):
        a[i] = mirror - a[i]
    Gwyddion.dump.write(data, filename)

try:
    args = sys.argv[1:]
    function = globals()[args.pop(0)]
    assert callable(function)
except (IndexError, KeyError, AssertionError):
    raise "Plug-in has to be called from Gwyddion plugin-proxy."

function(args)
