#!/usr/bin/python
# @(#) $Id$
# A very simple Gwyddion plug-in example in Python.
# Written by Yeti <yeti@gwyddion.net>.  Public domain.
import sys, Gwyddion

# Plug-in information.
run_modes = 'noninteractive', 'with_defaults'

plugin_info = """\
divide_by_2
/_Test/_Divide by 2 (Python)
""" + ' '.join(run_modes)

def register(args):
    print plugin_info

def run(args):
    run_mode = args.pop(0)
    assert run_mode in run_modes

    filename = args.pop(0)
    data = Gwyddion.dump.read(filename)
    dfield = data['/0/data']['data']
    for i in range(len(dfield)):
        dfield[i] /= 2.0
    Gwyddion.dump.write(data, filename)

try:
    args = sys.argv[1:]
    function = globals()[args.pop(0)]
    assert callable(function)
except (IndexError, KeyError, AssertionError):
    raise "Plug-in has to be called from Gwyddion plugin-proxy."

function(args)
