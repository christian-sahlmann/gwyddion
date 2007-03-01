#!/usr/bin/python
import re, sys, shutil, os

macro_name = 'GWY_RELOC_SOURCE'
line_length = 78
indent_step = 4
quiet = False

match_type = type(re.match('a', 'a'))

type_fields = {
    'GwyEnum': [ 'name', 'value' ],
}

type_flattened = {
    'GwyEnum': 'GwyFlatEnum'
}

def warn(message, lineno=-1, blockno=-1):
    if lineno > -1:
        message += ' at line %d' % lineno
    if blockno > -1:
        message += ' (block #%d)' % blockno
    sys.stderr.write(message + '\n')

def die(message, lineno=-1, blockno=-1):
    warn(message, lineno, blockno)
    sys.exit(1)

def backup(filename, suffix='~'):
    """Create a copy of file with suffix ('~' by default) appended."""
    try:
        shutil.copyfile(filename, filename + suffix)
        return True
    except IOError:
        return False

def backup_write_diff(filename, text):
    """Write text to a file, creating a backup and printing the diff."""
    rundiff = backup(filename)
    file(filename, 'w').write(text)
    if not quiet:
        sys.stdout.write(filename + '\n')
    if rundiff:
        fh = os.popen('diff "%s~" "%s"' % (filename, filename), 'r')
        diff = fh.readlines()
        fh.close()
        if diff:
            if quiet:
                sys.stdout.write(filename + '\n')
            sys.stdout.write(''.join(diff))

c_unescape_map = {
    't': '\t', 'n': '\n', 'r': '\r', '"': '"', 'b': '\b',
    'a': '\a', 'f': '\f', 'v': '\v', 'e': '\x1b', '0': '\0'
}
c_escape_map = dict([(v, k) for k, v in c_unescape_map.items()])

def c_unescape_char(s):
    if type(s) == match_type:
        s = s.group()
    assert s[0] == '\\'
    s = s[1:]
    if s.startswith('x') and len(s) == 3:
        return chr(int(s[1:], 16))
    if c_unescape_map.has_key(s):
        return c_unescape_map[s]
    if s[0] in '01234567' and len(s) == 3:
        return chr(int(s, 8))
    warn("Unknown character escape `\\%s'" % s)
    return s

def c_unescape(s):
    return re.sub(r'\\(?:x[0-9a-f]+\|[0-7]{3}|.)', c_unescape_char, s)

def c_escape_char(s):
    if s != '\0' and c_escape_map.has_key(s):
        return '\\' + c_escape_map[s]
    o = ord(s)
    if (o >= 32 and o < 127) or o >= 128:
        return s
    return '\\%03o' % o

def c_escape(s):
    return ''.join([c_escape_char(x) for x in s])

def wrap_string(s, width):
    lines = []
    l = ''
    for x in s:
        ex = c_escape_char(x)
        # Require l to be nonempty to always add at least one character
        if l and len(l + ex) + 2 >= width and ord(x) < 128:
            lines.append('"' + l + '"')
            l = ''
        l += ex
    if l:
        lines.append('"' + l + '"')
    return lines

class Block:
    decl_re = re.compile(r'^(?P<modif>(?:(?:static|const)\s+)*)'
                         r'(?P<type>\w+)\s+(?P<name>\w+)\s*\[\]\s*=\s+\{$')
    data_re = re.compile(r'^\{\s*(?P<items>.*?),?\s*\},?$')
    item_re = re.compile(r'(?P<item>"(?:"[^"\\]|\\"|\\\\)*"|[^,]+),\s*')
    end_re = re.compile(r'^\};$')
    struct_re = re.compile(r'^\s*/\*\s*@flat:\s*(?P<name>\w+)\s*\*/\s*$')
    fields_re = re.compile(r'^\s*/\*\s*@fields:\s*'
                           r'(?P<fields>[a-zA-Z0-9_, ]+?)\s*\*/\s*$')

    INT, STRING = range(2)

    def __init__(self, lineno=0, blockno=-1):
        self.lineno = lineno
        self.blockno = blockno

    def read(self, lines):
        decl_seen = False
        end_seen = False
        failed = False
        self.data = []
        self.names = []
        self.flat = None
        orig_lineno = self.lineno
        for line in lines:
            self.lineno += 1
            if failed:
                continue

            m = re.match(Block.struct_re, line)
            if m:
                if self.flat:
                    warn("Overriding previous flat name with `%s'" % line)
                self.flat = m.group('name')
                continue

            m = re.match(Block.fields_re, line)
            if m:
                if self.names:
                    warn("Overriding previous field names with `%s'" % line)
                self.names = re.split(r'[, \t]+', m.group('fields'))
                continue

            line = re.sub(r'/\*.*\*/\s*$', '', line)
            indent = re.match(r'\s*', line).group(0)
            line = line.strip()
            if not line:
                continue

            if end_seen:
                warn("Unexpected trailing data `%s'" % line)
                continue

            if not decl_seen:
                m = Block.decl_re.match(line)
                if not m:
                    warn("Cannot parse declaration `%s'" % line)
                    failed = True
                    continue
                decl_seen = True
                self.type = m.group('type')
                self.name = m.group('name')
                self.modif = m.group('modif')
                self.indent = indent
                continue

            m = Block.end_re.match(line)
            if m:
                end_seen = True
                continue

            m = Block.data_re.match(line)
            if not m:
                warn("Cannot parse data items `%s'" % line)
                failed = True
                continue
            record = []
            for item in re.finditer(Block.item_re, m.group('items') + ','):
                record.append(item.group('item'))
            self.data.append(record)

        return self.lineno - orig_lineno

    def _col_classify(self, item):
        if item.startswith('"') or item == 'NULL':
            return Block.STRING
        else:
            return Block.INT

    def check_fields(self):
        self.fields = None
        for record in self.data:
            coltypes = tuple([Block._col_classify(self, item)
                              for item in record])
            if not self.fields:
                self.fields = coltypes
            elif coltypes != self.fields:
                warn("Records do not have uniform field types")
                self.fields = None
                return False
            for i, item in enumerate(record):
                if self.fields[i] == Block.STRING:
                    if record[i] == 'NULL':
                        record[i] = None
                    else:
                        record[i] = c_unescape(item[1:-1])
                        if record[i].find('\0') > -1:
                            warn("NUL character found")
                            self.fields = None
                            return False


        if len(self.names) != len(self.fields):
            self.names = []

        if not self.flat and type_flattened.has_key(self.type):
            self.flat = type_flattened[self.type]

        if self.names:
            return True

        if type_fields.has_key(self.type):
            assert len(type_fields[self.type]) == len(self.fields)
            self.names = type_fields[self.type]
        else:
            warn("Field names are not known for type `%s'" % self.type)
            self.names = ['f' + str(x+1) for x in range(len(self.fields))]

        return True

    def _add_indent(self, s, indent):
        if s:
            return indent + s
        return s

    def format_flat(self):
        output = []
        self.strings = [[] for x in range(len(self.fields))]
        lenghts = [0 for x in range(len(self.fields))]
        for record in self.data:
            cols = []
            for i, item in enumerate(record):
                if self.fields[i] == Block.STRING:
                    s = record[i]
                    if s == None:
                        d = '-1'
                    else:
                        d = str(lenghts[i])
                        lenghts[i] += len(s) + 1
                elif self.fields[i] == Block.INT:
                    s = ''
                    d = record[i]
                cols.append(d)
                if s != None:
                    self.strings[i].append(s)
            output.append(cols)

        self.strings = ['\0'.join(x) for x in self.strings]
        maxw = line_length-1 - len(self.indent) - indent_step
        indent = ' ' * indent_step
        lines = ['/* This code block was GENERATED by flatten.py.',
                 '   When you edit %s[] data above,' % self.name,
                 '   re-run flatten.py SOURCE.c. */']
        for i, s in enumerate(self.strings):
            if self.fields[i] == Block.STRING:
                name = self.name + '_' + self.names[i]
                lines += ([self.modif + 'gchar ' + name + '[] =']
                          + [indent + x for x in wrap_string(s, maxw)])
                lines[-1] += ';'
                lines.append('')

        if self.flat:
            lines.append(self.modif + self.flat + ' ' + self.name + '[] = {')
        else:
            lines.append(self.modif + 'struct {')
            for x in self.names:
                lines.append(indent + 'gint ' + x + ';')
            lines.append('}')
            lines.append(self.name + '[] = {')
        for record in output:
            lines.append(indent + '{ ' + ', '.join(record) + ' },')
        lines.append('};')

        self.output = [self._add_indent(x, self.indent) + '\n' for x in lines]
        return self.output

source_re = re.compile(r'^\s*#\s*ifdef\s+' + macro_name + r'\s*$')
replace_re = re.compile(r'^\s*#\s*else\s*(?:/\*.*\*/\s*)?$')
end_re = re.compile(r'^\s*#\s*endif\s*(?:/\*.*\*/\s*)?$')
for filename in sys.argv[1:]:
    lines = []
    in_source = False
    in_replace = False
    fh = file(filename)
    for line in fh:
        if in_source:
            lines.append(line)
            if replace_re.match(line):
                b = Block()
                b.read(source_lines)
                if b.check_fields():
                    lines += b.format_flat()
                in_source = False
                in_replace = True
            else:
                source_lines.append(line)
        elif in_replace:
            if end_re.match(line):
                lines.append(line)
                in_replace = False
        else:
            lines.append(line)
            if source_re.match(line):
                source_lines = []
                in_source = True
    fh.close()
    backup_write_diff(filename, ''.join(lines))

