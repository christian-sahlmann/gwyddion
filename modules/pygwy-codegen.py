#!/usr/bin/python
import sys

# Add codegen path to our import path
i = 1
codegendir = None
while i < len(sys.argv):
    arg = sys.argv[i]
    if arg == '--codegendir':
        del sys.argv[i]
        codegendir = sys.argv[i]
        del sys.argv[i]
    elif arg.startswith('--codegendir='):
        codegendir = arg.split('=', 2)[1]
        del sys.argv[i]
    else:
        i += 1
if codegendir:
    sys.path.insert(0, codegendir)
del codegendir

# Load it
from codegen import *

# Extend argtypes
arg = argtypes.IntArg()
argtypes.matcher.register('GQuark', arg)
del arg

# Run codegen
sys.exit(main(sys.argv))
