#!/usr/bin/python
# @(#) $Id$
import re, os, sys, shutil, glob, time

# The value part of a Makefile assignment with line continuations
re_listend = re.compile(r'\s*\\\n\s*')

# Parse symbols in nm output
re_nm = re.compile(r'(?P<addr>[a-z0-9 ]+) (?P<type>[-A-Za-z?]) (?P<symbol>\w+)')

# .Match gwt template <[[:FOO:]]>
re_template = re.compile(r'<\[\[:(?P<name>\w+):\]\]>')

# RCS id line (to remove them from generated files)
re_rcsid = re.compile(r'^# @\(#\) \$(Id).*', re.MULTILINE)

object_rule = """\
%s.obj:%s
\t$(CC) $(CFLAGS) -GD -c $(%s_CFLAGS) %s.c
"""

mod_dll_rule = """\
%s.dll: %s.obj
\t$(LINK32) %s.obj $(MOD_LINK) $(WIN32LIBS) $(LDFLAGS) /out:%s.dll /dll /implib:%s.lib
"""

mo_inst_rule = """\
\t$(INSTALL) %s.gmo "$(DEST_DIR)\\locale\\%s\\LC_MESSAGES\\gwyddion.mo"\
"""

me = 'utils/update-msvc.py'
top_dir = os.getcwd()

quiet = len(sys.argv) > 1 and sys.argv[1] == '-q'

def underscorize(s):
    """Change all nonaplhanumeric characters to underscores"""
    return re.sub(r'\W', r'_', s)

def format_list(l, prefix=''):
    """Format a list with line continuations."""
    return ' \\\n\t'.join([prefix] + l)

def backup(filename, suffix='~'):
    """Create a copy of file with suffix ('~' by default) appended."""
    try:
        shutil.copyfile(filename, filename + suffix)
        return True
    except IOError:
        return False

def get_file(filename):
    """Get the contents of a file as a string."""
    fh = file(filename, 'r')
    text = fh.read()
    fh.close()
    return text

def write_file(filename, text):
    """Write text, which can be a string or list of lines, to a file."""
    if type(text) == type([]):
        t = '\n'.join(text) + '\n'
    else:
        t = text
    # Convert from any mix of LF and CRLF to CRLF
    t = t.replace('\r', '').replace('\n', '\r\n')
    fh = file(filename, 'w')
    fh.write(t);
    fh.close()

def print_filename(filename):
    rcwd = os.getcwd()[len(top_dir)+1:]
    print os.path.join(rcwd, filename)

def backup_write_diff(filename, text):
    """Write text to a file, creating a backup and printing the diff."""
    rundiff = backup(filename)
    write_file(filename, text)
    if not quiet:
        print_filename(filename)
    if rundiff:
        fh = os.popen('diff "%s~" "%s"' % (filename, filename), 'r')
        diff = fh.readlines()
        fh.close()
        if diff:
            if quiet:
                print_filename(filename)
            sys.stdout.write(''.join(diff))

def get_list(text, name, assignment_op='='):
    """Parse Makefile assignment with line continuation, return lists."""
    r = re.compile(r'^\s*%s\s*%s\s*(?P<list>(?:.*\\\n)*.*)'
                   % (name, assignment_op), re.M)
    m = r.search(text)
    if not m:
        return []
    m = m.group('list')
    m = re_listend.sub(' ', m).strip().split(' ')
    return m

def fix_suffixes(lst, suffix, replacewith=''):
    """Replace (remove) string suffixes in a list, retuning new list."""
    if not suffix:
        return lst
    sl = len(suffix)
    newlst = []
    for x in lst:
        if x.endswith(suffix):
            x = x[:-sl] + replacewith
        newlst.append(x)
    return newlst

def expand_make_vars(lst, makefile):
    """Rudimentary Makefile variable expansion."""
    if not makefile:
        return lst
    newlst = []
    r = re.compile(r'\$\((?P<name>\w+)\)')
    for x in lst:
        while True:
            m = r.search(x)
            if not m:
                newlst.append(x)
                break
            name = m.group('name')
            value = get_list(makefile, name)
            if len(value) > 1:
                print 'WARNING: Complex expansions not supported (%s)' % name
            x = ' '.join(value)
    return newlst

def get_object_symbols(filename, symtype='T'):
    """Get symbols of specified type (by default 'T') from an object file."""
    fh = os.popen('nm -p -t x %s' % filename, 'r')
    syms = [re_nm.match(x) for x in fh.readlines()]
    fh.close()
    syms = [x for x in syms if x]
    syms = [x.group('symbol') for x in syms if x.group('type') == symtype]
    syms = [x for x in syms if not x.startswith('_')]
    syms.sort()
    return syms

def get_file_deps(name, sources=None):
    """Get the list of object file (basename given) non-sys dependencies."""
    if sources:
        sources = name, sources + '-' + name
    else:
        sources = (name,)

    for x in sources:
        for ext in 'o', 'lo':
            depfile = os.path.join('.deps', x + '.P' + ext)
            if not os.access(depfile, os.R_OK):
                continue

            x = '(?:(?:\.libs/)?' + x + '\.l?o\s*)+'
            l = get_list(get_file(depfile), x, ':')
            l.sort()
            return [x for x in l if not x.startswith('/')]
    print 'WARNING: No deps for %s.c' % name
    return []

def make_lib_defs(makefile):
    """Create a .def file with exported symbols for each libtool library."""
    libraries = fix_suffixes(get_list(makefile, 'lib_LTLIBRARIES'), '.la')
    for l in libraries:
        syms = get_object_symbols('.libs/%s.so' % l)
        syms = ['EXPORTS'] + ['\t' + x for x in syms]
        backup_write_diff('%s.def' % l, syms)

def expand_template(makefile, name, supplementary=None):
    """Get expansion of specified template, taking information from Makefile.

    SELF: defines TOP_SRCDIR, LIBRARY, LIBDIR
    DATA: install-data rule, created from foo_DATA
    LIB_HEADERS: install rules, filled from lib_LTLIBRARIES, fooinclude_HEADERS
    LIB_OBJECTS: this variable, filled from lib_LTLIBRARIES, foo_SOURCES
    LIB_OBJ_RULES: .c -> .obj rules, filled from lib_LTLIBRARIES, foo_SOURCES
    PRG_OBJECTS: this variable, filled from bin_PROGRAMS, foo_SOURCES
    PRG_OBJ_RULES: .c -> .obj rules, filled from bin_PROGRAMS, foo_SOURCES
    MODULES: this variable, filled from foo_LTLIBRARIES
    MOD_OBJ_RULES: .c -> .obj rules, filled from foo_LTLIBRARIES, foo_SOURCES
    MOD_DLL_RULES: .obj -> .dll rules, filled from foo_LTLIBRARIES
    MO_INSTALL_RULES: installdirs and install-mo rules, filled from LINGUAS"""

    if name == 'SELF':
        if srcpath:
            srcdir = '\\'.join(['..'] * len(srcpath))
            s = ['TOP_SRCDIR = ' + srcdir]
        else:
            s = ['TOP_SRCDIR = .']

        libraries = fix_suffixes(get_list(makefile, 'lib_LTLIBRARIES'), '.la')
        if libraries:
            assert len(libraries) == 1
            s.append('LIBDIR = ' + srcpath[-1])
            l = libraries[0]
            s.append('LIBRARY = ' + l)
        return '\n'.join(s)
    elif name == 'DATA':
        lst = get_list(makefile, '\w+_DATA')
        list_part = name + ' =' + format_list(lst)
        inst_part = [('$(INSTALL) %s "$(DEST_DIR)\$(DATA_TYPE)"' % x)
                     for x in lst]
        inst_part = '\n\t'.join(['install-data: data'] + inst_part)
        return list_part + '\n\n' + inst_part
    elif name == 'LIB_HEADERS':
        libraries = fix_suffixes(get_list(makefile, 'lib_LTLIBRARIES'), '.la')
        lst = []
        for l in libraries:
            ul = underscorize(l)
            lst += get_list(makefile, '%sinclude_HEADERS' % ul)
        list_part = name + ' =' + format_list(lst)
        inst_part = [('$(INSTALL) %s "$(DEST_DIR)\include\$(LIBDIR)"' % x)
                     for x in lst]
        inst_part = '\n\t'.join(['install-headers:'] + inst_part)
        return list_part + '\n\n' + inst_part
    elif name == 'LIB_OBJECTS':
        libraries = get_list(makefile, 'lib_LTLIBRARIES')
        lst = []
        for l in libraries:
            ul = underscorize(l)
            lst += fix_suffixes(get_list(makefile, '%s_SOURCES' % ul),
                                '.c', '.obj')
        return name + ' =' + format_list(lst)
    elif name == 'LIB_OBJ_RULES':
        libraries = get_list(makefile, 'lib_LTLIBRARIES')
        lst = []
        for l in libraries:
            ul = underscorize(l)
            for x in fix_suffixes(get_list(makefile, '%s_SOURCES' % ul), '.c'):
                deps = format_list(get_file_deps(x, ul))
                lst.append(object_rule % (x, deps, 'LIB', x))
        return  '\n'.join(lst)
    elif name == 'PRG_OBJECTS':
        programs = get_list(makefile, 'bin_PROGRAMS')
        lst = []
        for p in programs:
            up = underscorize(p)
            lst += fix_suffixes(get_list(makefile, '%s_SOURCES' % up),
                                '.c', '.obj')
        return name + ' =' + format_list(lst)
    elif name == 'PRG_OBJ_RULES':
        programs = get_list(makefile, 'bin_PROGRAMS')
        lst = []
        for p in programs:
            up = underscorize(p)
            for x in fix_suffixes(get_list(makefile, '%s_SOURCES' % up), '.c'):
                deps = format_list(get_file_deps(x))
                lst.append(object_rule % (x, deps, 'PRG', x))
        return  '\n'.join(lst)
    elif name == 'MODULES':
        mods = get_list(makefile, r'\w+_LTLIBRARIES')
        mods = expand_make_vars(mods, supplementary)
        mods = fix_suffixes(fix_suffixes(mods, '.la', '.dll'), ')', ').dll')
        return name + ' =' + format_list(mods)
    elif name == 'MOD_OBJ_RULES':
        mods = get_list(makefile, r'\w+_LTLIBRARIES')
        mods = expand_make_vars(mods, supplementary)
        lst = []
        for m in mods:
            um = underscorize(m)
            for x in get_list(makefile, '%s_SOURCES' % um):
                # Ignore header files in SOURCES
                if not x.endswith('.c'):
                    continue
                x = x[:-2]
                deps = format_list(get_file_deps(x, um))
                lst.append(object_rule % (x, deps, 'MOD', x))
        return  '\n'.join(lst)
    elif name == 'MOD_DLL_RULES':
        mods = get_list(makefile, r'\w+_LTLIBRARIES')
        mods = expand_make_vars(mods, supplementary)
        mods = fix_suffixes(mods, '.la')
        lst = []
        for m in mods:
            lst.append(mod_dll_rule % (m, m, m, m, m))
        return  '\n'.join(lst)
    elif name == 'MO_INSTALL_RULES':
        pos = [l.strip() for l in file('LINGUAS')]
        pos = [l for l in pos if l and not l.strip().startswith('#')]
        assert len(pos) == 1
        pos = re.split(r'\s+', pos[0])
        if not pos:
            return
        lst = ['installdirs:', '\t-@mkdir "$(DEST_DIR)\\locale"']
        for p in pos:
            lst.append('\t-@mkdir "$(DEST_DIR)\\locale\\%s"' % p)
            lst.append('\t-@mkdir "$(DEST_DIR)\\locale\\%s\\LC_MESSAGES"' % p)
        lst.append('')
        lst.append('install-mo:')
        for p in pos:
            lst.append(mo_inst_rule % (p, p))
        return '\n'.join(lst)
    print 'WARNING: Unknown template %s' % name
    return ''

def fill_templates(makefile):
    """Expand .gwt files in current directory, taking information from Makefile."""
    templates = glob.glob('*.gwt')
    templates = fix_suffixes(templates, '.gwt')
    for templ in templates:
        text = orig = get_file(templ + '.gwt')
        text = re_rcsid.sub('# This file was GENERATED from %s.gwt by %s.'
                            % (templ, me), text)
        m = re_template.search(text)
        while m:
            text = text[:m.start()] \
                   + expand_template(makefile, m.group('name'), orig) \
                   + text[m.end():]
            m = re_template.search(text)
        backup_write_diff(templ, text)

def process_one_dir(makefile):
    make_lib_defs(makefile)
    fill_templates(makefile)

def recurse(each):
    cwd = os.getcwd()
    try:
        makefile = get_file('Makefile.am')
    except IOError:
        return
    each(makefile)
    subdirs = get_list(makefile, 'SUBDIRS')
    for s in subdirs:
        os.chdir(s)
        srcpath.append(s)
        recurse(each)
        srcpath.pop()
        os.chdir(cwd)

def check_make_all():
    """Die noisily if 'make -q all' does not succeed."""
    return True
    # FIXME: For some obscure automake reason, perhaps related to bulding
    # BUILT_SOURCES with += operator, this fails even if make all passed OK
    status = os.system('make -q -s all >/dev/null')
    if not status:
        return True
    sys.stderr.write('%s: project build did not succeed '
                     '(or it was not attempted)\n' % me)
    return False

def check_completness(config):
    """Check if optional features that can affect completnes are enabled."""
    must_have = 'HAVE_TIFF', 'HAVE_XML2', 'HAVE_GTKGLEXT'
    text = get_file(config)
    ok = True
    for x in must_have:
        if re.search(r'^#define\s+%s\s+' % x, text, re.M):
            continue
        sys.stderr.write('%s: %s is not defined in %s\n' % (me, x, config))
        ok = False
    return ok

configure = get_file('configure.ac')
ok = check_completness('config.h')
ok *= check_make_all()
if not ok:
    sys.stderr.write('%s: generated files would be incomplete, quitting\n' % me)
    sys.exit(1)

srcpath = []
recurse(process_one_dir)

cwd = os.getcwd()
os.chdir('po')
srcpath.append('po')
fill_templates(configure)
srcpath.pop()
os.chdir(cwd)

