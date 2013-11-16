#!/usr/bin/python
import sys, re

header = '''\
<?xml version="1.0" encoding="UTF-8"?>
<!--

 This is a ''' + 'GENERATED' + ''' file.

-->
<language id="pygwy" _name="Python + gwy" version="2.0" _section="Sources">
  <metadata>
    <property name="mimetypes">text/x-python;application/x-python</property>
    <property name="globs">*.py</property>
    <property name="line-comment-start">#</property>
  </metadata>

  <styles>
    <style id="class" _name="Gwy Class" map-to="python:builtin-object"/>
    <style id="function" _name="Gwy Function" map-to="python:builtin-function"/>
    <style id="enum-value" _name="Gwy Enum Value" map-to="python:builtin-constant"/>
  </styles>

  <definitions>
    <context id="pygwy-keywords">
      <include>
'''

footer = '''\
      </include>
    </context>

    <context id="pygwy" class="no-spell-check">
      <include>
        <context ref="pygwy-keywords"/>
        <context ref="python:python"/>
      </include>
    </context>
  </definitions>
</language>
'''

def wrap(symbols, cid, styleref):
    symbols = '\n'.join('          <keyword>' + x + '</keyword>'
                        for x in symbols)
    intro = '<context id="%s" style-ref="%s">' % (cid, styleref)
    return ('        ' + intro + '\n'
            + symbols + '\n'
            '        </context>\n')

text = sys.stdin.read()

sys.stdout.write(header)

classes = []
for m in re.finditer(r'(?ms)^\(define-(?:object|enum|flags) (?P<name>\w+)$',
                     text):
    classes.append(m.group('name'))
sys.stdout.write(wrap(classes, 'classes', 'class'))

functions = []
for m in re.finditer(r'(?ms)^\(define-function (?P<name>\w+)$', text):
    functions.append(m.group('name'))
sys.stdout.write(wrap(functions, 'functions', 'function'))

constants = []
for m in re.finditer(r'(?ms)^\s+\(values(?P<values>.*?)^\s+\)$', text):
    for mm in re.finditer(r'(?ms)^\s+\'\("[^"]+" "GWY_(?P<name>\w+)"\)$',
                          m.group('values')):
        constants.append(mm.group('name'))
sys.stdout.write(wrap(constants, 'enum-values', 'enum-value'))

sys.stdout.write(footer)
