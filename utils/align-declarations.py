#!/usr/bin/python
# align-declarations.py written by Yeti.
# This file is in the public domain.
import re, copy, sys

help = """\
Aligns declarations in C code
Usage: align-declarations.py <INPUT-FILE >OUTPUT-FILE

Currently, three simple list types can be aligned:
- function prototypes
- array initializers
- whitespace separated columns
Run with --examples to see some examples.
"""

examples = (
"""\
static gboolean module_register          (const gchar *name);

static gboolean indent_analyze(GwyContainer *data, GwyRunType run);

static gboolean         indent_analyze_dialog(GwyContainer *data,
IndentAnalyzeArgs *args);
static void             dialog_update   (IndentAnalyzeControls *controls,
                                                  IndentAnalyzeArgs *args);

static void             plane_correct_cb(GtkWidget *item, IndentAnalyzeControls
*controls);
static void             indentor_changed_cb(GtkWidget *item,
IndentAnalyzeControls *controls);
static void     set_mask_at (GwyDataField *mask, gint x, gint y, gdouble  m,
gint how );
static void     level_data (IndentAnalyzeControls *c);
""",
"""\
    { N_("Vickers"),    GWY_INDENTOR_VICKERS },
     { N_("Berkovich"),  GWY_INDENTOR_BERKOVICH },
    { N_("Berkovich (modified)"),  GWY_INDENTOR_BERKOVICH_M },
    { N_("Knoop"),      GWY_INDENTOR_KNOOP, },
    { N_("Brinell"),   GWY_INDENTOR_BRINELL, },
     { N_("Cube corner"), GWY_INDENTOR_CUBECORNER, },
      { N_("Rockwell"),   GWY_INDENTOR_ROCKWELL },
""",
"""\
    GWY_FFT_OUTPUT_REAL_IMG = 0,
    GWY_FFT_OUTPUT_MOD_PHASE  = 1,
    GWY_FFT_OUTPUT_REAL = 2,
    GWY_FFT_OUTPUT_IMG = 3,
    GWY_FFT_OUTPUT_MOD = 4,
    GWY_FFT_OUTPUT_PHASE = 5
"""
)

class Columnizer:
    def __init__(self):
        self.re = re.compile(self.__class__.regexp)

    def format(self, w):
        lines = [''.join([self.cols[i][x].ljust(w[i][x])
                          for x in range(len(w[i]))]).rstrip()
                 for i in range(len(w))]
        return '\n'.join(lines)

class FuncColumnizer(Columnizer):
    regexp = (r'(?s)(?P<type>.*?)\b(?P<name>[_a-zA-Z]\w*)\s*'
              r'(?P<args>\(.*?\);)\s*')

    def fix(self, s):
        return re.sub(r'\s+', ' ', s).strip()

    def feed(self, text):
        self.cols = []
        for m in self.re.finditer(text):
            args = [self.fix(x) for x in m.group('args').split(',')]
            for i in range(0, len(args)-1):
                args[i] = args[i] + ','
            typ = re.sub(r'\s+\*$', '*', self.fix(m.group('type'))) + ' '
            line = [typ, m.group('name'), args[0]]
            self.cols.append(line)
            for x in args[1:]:
                self.cols.append(['', '', ' ' + x])

        self.widths = [[len(x) for x in line] for line in self.cols]
        return self.widths

class ArrayColumnizer(Columnizer):
    regexp = r'{\s*(?P<body>.*?),?\s*}'

    def fix(self, s):
        return s.strip() + ', '

    def feed(self, text):
        indent = re.match(r'\A(\s*)', text).groups()[0]
        self.cols = []
        for m in self.re.finditer(text):
            g = m.group('body').split(',')
            decls = [indent + '{ '] + [self.fix(x) for x in g] + ['},']
            self.cols.append(decls)

        self.widths = [[len(x) for x in line] for line in self.cols]
        return self.widths

class SimpleColumnizer(Columnizer):
    regexp = r'(?m)^(?P<indent>\s*)(?P<text>.+)$'

    def fix(self, s):
        return re.sub(r'$', ' ', s.strip())

    def feed(self, text):
        self.cols = []
        indent = None
        for m in self.re.finditer(text):
            if indent is None:
                indent = m.group('indent')
            decls = [self.fix(x) for x in m.group('text').split()]
            decls[0:0] = [indent]
            decls[-1] = decls[-1].rstrip()
            self.cols.append(decls)

        self.widths = [[len(x) for x in line] for line in self.cols]
        return self.widths

class Aligner:
    def align(self, w, width=-1):
        self.w = w
        self.rows = len(w)
        self.cols = len(w[0])
        # Minimal line size
        self.wrmin = [sum(w[i]) for i in range(self.rows)]
        self.wmin = max(self.wrmin)
        if width > -1 and width < self.wmin:
            raise RuntimeError, "Minimum width larger than requested"
        # Natural col widths
        self.wcnat = [max([w[i][j] for i in range(self.rows)])
                      for j in range(self.cols)]
        self.wnat = sum(self.wcnat)
        if width < 0:
            self.targetw = self.wnat
        else:
            self.targetw = width
        self._align()
        return self.w

class TrivialAligner(Aligner):
    def _align(self):
        for i in range(self.rows):
            self.w[i] = copy.copy(self.wcnat)

def align_string(s):
    stripped = s.strip()
    if stripped.startswith('{'):
        c = ArrayColumnizer()
    elif stripped.endswith(');'):
        c = FuncColumnizer()
    else:
        c = SimpleColumnizer()

    a = TrivialAligner()
    return c.format(a.align(c.feed(s)))

if len(sys.argv) > 1:
    if sys.argv[1] in ('--help', '-h'):
        print help
        sys.exit(0)
    if sys.argv[1] in ('--examples'):
        for x in examples:
            print "===== INPUT ====="
            print x
            print "===== OUTPUT ====="
            print align_string(x)
            print
        sys.exit(0)
    for x in sys.argv[1:]:
        print align_string(file(x).read())
else:
    print align_string(sys.stdin.read())
