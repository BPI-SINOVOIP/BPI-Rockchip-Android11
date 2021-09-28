#!/usr/bin/env python

import os
import subprocess
import sys
from antlr4 import *
from gnparser.gnLexer import gnLexer
from gnparser.gnParser import gnParser
from gnparser.gnListener import gnListener
from string import Template

DBG = False

# Reformat the specified Android.bp file
def _bpFmt(filename):
  ## NOTE: bpfmt does not set error code even when the bp file is illegal.
  print subprocess.check_output(["bpfmt", "-w", filename])

def _bpList(entries):
  return '[' + ",".join(['"' + x + '"' for x in entries]) + ']'

# Write an Android.bp in the simpler format used by v8_libplatform and
# v8_libsampler
def _writeBP(filename, module_name, sources):
  if not sources:
    raise ValueError('No sources for ' + filename)

  with open(filename, 'w') as out:
    out.write(Template('''
      // GENERATED, do not edit
      // for changes, see genmakefiles.py
      cc_library_static {
          name: "$module_name",
          defaults: ["v8_defaults"],
          srcs: $srcs,
          local_include_dirs: ["src", "include"],
          apex_available: [
              "com.android.art.debug",
              "com.android.art.release",
          ],
      }
    ''').substitute({'module_name': module_name, 'srcs' : _bpList(sorted(sources))}))

  _bpFmt(filename)

def _writeFileGroupBP(filename, module_name, sources):
  if not sources:
    raise ValueError('No sources for ' + filename)

  with open(filename, 'w') as out:
    out.write(Template('''
      // GENERATED, do not edit
      // for changes, see genmakefiles.py
      filegroup {
          name: "$module_name",
          srcs: $srcs,
      }
    ''').substitute({'module_name': module_name, 'srcs' : _bpList(sorted(sources))}))

  _bpFmt(filename)


def _writeV8BaseBP(getSourcesFunc):
  sources = getSourcesFunc(None)
  if not sources:
    raise ValueError('Must specify v8_base target properties')
  sources.add('src/heap/base/stack.cc')
  sources.add('src/heap/base/worklist.cc')
  sources.add('third_party/zlib/google/compression_utils_portable.cc')

  arm_src = list(getSourcesFunc('arm') - sources)
  arm64_src = list(getSourcesFunc('arm64') - sources)
  x86_src = list(getSourcesFunc('x86') - sources)
  x86_64_src = list(getSourcesFunc('x64') - sources)

  filename = 'Android.base.bp'
  with open(filename, 'w') as out:
    out.write(Template('''
      // GENERATED, do not edit
      // for changes, see genmakefiles.py
      cc_library_static {
          name: "v8_base",
          defaults: ["v8_defaults", "v8_torque_headers"],
          srcs: $srcs,
          arch: {
              arm: {
                  srcs: $arm_src,
                  generated_sources: [
                      "v8_torque_file_cc_32",
                  ],
              },
              arm64: {
                  srcs: $arm64_src,
                  generated_sources: [
                      "v8_torque_file_cc",
                  ],
              },
              x86: {
                  srcs: $x86_src,
                  generated_sources: [
                      "v8_torque_file_cc_32",
                  ],

              },
              x86_64: {
                 srcs: $x86_64_src,
                  generated_sources: [
                      "v8_torque_file_cc",
                  ],
              },
          },
          local_include_dirs: ["src", "include", "third_party/zlib",],
          generated_headers: ["v8_generate_bytecode_builtins_list"],
          sanitize: {
              cfi: true,
              blacklist: "./tools/cfi/blacklist.txt",
          },
          apex_available: [
              "com.android.art.debug",
              "com.android.art.release",
          ],
      }
    ''').substitute({'srcs': _bpList(sorted(sources)),
                     'arm_src': _bpList(sorted(arm_src)),
                     'arm64_src': _bpList(sorted(arm64_src)),
                     'x86_src': _bpList(sorted(x86_src)),
                     'x86_64_src': _bpList(sorted(x86_64_src)),
                    }))

  _bpFmt(filename)


def _writeLibBaseBP(sources):
  if not sources:
    raise ValueError('Must specify v8_libbase target properties')

  filename = 'Android.libbase.bp'
  with open(filename, 'w') as out:
    out.write(Template('''
      // GENERATED, do not edit
      // for changes, see genmakefiles.py
      cc_library_static {
          name: "v8_libbase",
          defaults: ["v8_defaults"],
          host_supported: true,
          srcs: $srcs,
          local_include_dirs: ["src"],
          target: {
              android: {
                  srcs: [
                      "src/base/debug/stack_trace_android.cc",
                      "src/base/platform/platform-linux.cc",
                  ],
              },
              host: {
                  srcs: [
                      "src/base/debug/stack_trace_posix.cc",
                      "src/base/platform/platform-linux.cc",
                  ],
                  cflags: ["-UANDROID"],
              },
          },
          apex_available: [
              "com.android.art.debug",
              "com.android.art.release",
          ],
      }
    ''').substitute({'srcs' : _bpList(sorted(sources))}))

  _bpFmt(filename)


def _writeTorqueBP(sources):
  if not sources:
    raise ValueError('Must specify torque_files target properties')
  genfiles = [
      "torque-generated/bit-fields.h",
      "torque-generated/builtin-definitions.h",
      "torque-generated/interface-descriptors.inc",
      "torque-generated/factory.cc",
      "torque-generated/factory.inc",
      "torque-generated/field-offsets.h",
      "torque-generated/class-verifiers.cc",
      "torque-generated/class-verifiers.h",
      "torque-generated/enum-verifiers.cc",
      "torque-generated/objects-printer.cc",
      "torque-generated/objects-body-descriptors-inl.inc",
#      "torque-generated/class-debug-readers.cc",
#      "torque-generated/class-debug-readers.h",
      "torque-generated/exported-macros-assembler.cc",
      "torque-generated/exported-macros-assembler.h",
      "torque-generated/csa-types.h",
      "torque-generated/instance-types.h",
      "torque-generated/runtime-macros.cc",
      "torque-generated/runtime-macros.h",
      "torque-generated/class-forward-declarations.h",
  ]

  intl_files = set([
    "src/objects/intl-objects.tq",
    "src/objects/js-break-iterator.tq",
    "src/objects/js-collator.tq",
    "src/objects/js-date-time-format.tq",
    "src/objects/js-display-names.tq",
    "src/objects/js-list-format.tq",
    "src/objects/js-locale.tq",
    "src/objects/js-number-format.tq",
    "src/objects/js-plural-rules.tq",
    "src/objects/js-relative-time-format.tq",
    "src/objects/js-segment-iterator.tq",
    "src/objects/js-segmenter.tq",
    "src/objects/js-segments.tq"
  ])
  sources = [s for s in sources if not s in intl_files]

  #sources = sources[:1]
  #sources = filter(lambda s:not s.startswith("test/"), sources)
  for tq in sources:
    filetq = tq.replace('.tq', '-tq')
    genfiles.append("torque-generated/" + filetq + "-csa.cc")
    genfiles.append("torque-generated/" + filetq + "-csa.h")
    genfiles.append("torque-generated/" + filetq + "-inl.inc")
    genfiles.append("torque-generated/" + filetq + ".cc")
    genfiles.append("torque-generated/" + filetq + ".inc")

  dirs = set(map(os.path.dirname, sources))
  cc_files = filter(lambda s:s.endswith(".cc"), genfiles)
  include_files = filter(lambda s:not s.endswith(".cc"), genfiles)

  filename = 'Android.torque.bp'
  with open(filename, 'w') as out:
    out.write(Template('''
      // GENERATED, do not edit
      // for changes, see genmakefiles.py
      filegroup {
          name: "v8_torque_src_files",
          srcs: $srcs,
      }

      genrule {
          name: "v8_torque_file",
          tools: ["v8_torque"],
          srcs: [":v8_torque_src_files"],
          cmd: $cmd,
          out: $include_files,
      }

      genrule {
          name: "v8_torque_file_cc",
          tools: ["v8_torque"],
          srcs: [":v8_torque_src_files"],
          cmd: $cmd,
          out: $cc_files,
      }

      genrule {
          name: "v8_torque_file_32",
          tools: ["v8_torque"],
          srcs: [":v8_torque_src_files"],
          cmd: $cmd32,
          out: $include_files,
      }

      genrule {
          name: "v8_torque_file_cc_32",
          tools: ["v8_torque"],
          srcs: [":v8_torque_src_files"],
          cmd: $cmd32,
          out: $cc_files,
      }
    ''').substitute({
                     'srcs' : _bpList(sources),
                     'cmd' : '''"mkdir -p $(genDir)/torque-generated/ && pushd . && cd $(genDir)/torque-generated/ && mkdir -p ''' + ' '.join(dirs) + ''' && popd && $(location v8_torque) -o $(genDir)/torque-generated/ -v8-root $$(pwd)/external/v8 $$(echo $(in) | sed 's/external.v8.//g'); true"''',
                     'cmd32' : '''"mkdir -p $(genDir)/torque-generated/ && pushd . && cd $(genDir)/torque-generated/ && mkdir -p ''' + ' '.join(dirs) + ''' && popd && $(location v8_torque) -o $(genDir)/torque-generated/ -v8-root $$(pwd)/external/v8 -m32 $$(echo $(in) | sed 's/external.v8.//g'); true"''',
                     'cc_files' : _bpList(cc_files),
                     'include_files' : _bpList(include_files)}))

  _bpFmt(filename)

def _expr_to_str(expr):
  val = expr.unaryexpr().primaryexpr()
  if val.String():
    return val.String().getText()[1:-1] ## Strip quotation marks around string
  elif val.Identifier():
    return val.Identifier().getText()
  else:
    if DBG: print 'WARN: unhandled primary expression'
    return None

class V8GnListener(gnListener):
    def __init__(self, target, arch, only_cc_files):
        super(gnListener, self).__init__()
        self._match = False
        self._depth = 0
        self._target = target
        self._arch = arch
        self._sources = []
        self._fixed_conditions = {
            'use_jumbo_build' : True,
            'use_jumbo_build==true' : True,
            'is_win' : False,
            'is_linux' : False,
            'v8_postmortem_support' : False,
            'v8_enable_i18n_support': False,
            '!v8_enable_i18n_support': True,
            'current_os!="aix"' : True,
            'is_posix||is_fuchsia' : True,
            'v8_current_cpu=="arm"' : arch == 'arm',
            'v8_current_cpu=="arm64"' : arch == 'arm64',
            'v8_current_cpu=="x86"' : arch == 'x86',
            'v8_current_cpu=="x64"' : arch == 'x64',
            'v8_current_cpu=="mips"||v8_current_cpu=="mipsel"' : arch == 'mips',
            'v8_current_cpu=="mips64"||v8_current_cpu=="mips64el"' : arch == 'mips64',
            'v8_current_cpu=="ppc"||v8_current_cpu=="ppc64"' : False,
            'v8_current_cpu=="s390"||v8_current_cpu=="s390x"' : False,
            'is_linux||is_chromeos||is_mac||is_ios||target_os=="freebsd"' : True,

        }
        self._only_cc_files = only_cc_files

    def _match_call_target(self, ctx):
      call_type = ctx.Identifier().getText()
      if not call_type in ['v8_source_set', 'v8_component', 'action']: return False
      call_name = _expr_to_str(ctx.exprlist().expr(0))
      return call_name == self._target

    def enterCall(self, ctx):
      if self._depth == 1 and self._match_call_target(ctx):
        self._match = True
        self._conditions = [] ## [(value, condition), ...]
        if DBG: print 'Found call', str(ctx.Identifier()), ctx.exprlist().getText()

    def exitCall(self, ctx):
      if self._match and self._match_call_target(ctx):
        self._match = False
        self._conditions = []
        if DBG: print 'Left call'

    def _extract_sources(self, ctx):
      op = ctx.AssignOp().getText()
      if not ctx.expr().unaryexpr().primaryexpr().exprlist():
        ## sources += check_header_includes_sources
        return
      srcs = map(_expr_to_str, ctx.expr().unaryexpr().primaryexpr().exprlist().expr())
      if self._only_cc_files:
        srcs = [x for x in srcs if x.endswith('.cc')]
      if DBG: print '_extract_sources: ', len(srcs), "condition:", self._conditions
      if op == '=':
        if self._sources:
          print "WARN: override sources"
        self._sources = srcs
      elif op == '+=':
        self._sources.extend(srcs)

    def _compute_condition(self, ctx):
      condition = ctx.expr().getText()
      if DBG: print '_extract_condition', condition
      if condition in self._fixed_conditions:
        result = self._fixed_conditions[condition]
      else:
        print 'WARN: unknown condition, assume False', condition
        self._fixed_conditions[condition] = False
        result = False
      if DBG: print 'Add condition:', condition
      self._conditions.append((result, condition))


    def enterCondition(self, ctx):
      if not self._match: return
      self._compute_condition(ctx)

    def enterElsec(self, ctx):
      if not self._match: return
      c = self._conditions[-1]
      self._conditions[-1] = (not c[0], c[1])
      if DBG: print 'Negate condition:', self._conditions[-1]

    def exitCondition(self, ctx):
      if not self._match: return
      if DBG: print 'Remove conditions: ', self._conditions[-1]
      del self._conditions[-1]

    def _flatten_conditions(self):
      if DBG: print '_flatten_conditions: ', self._conditions
      for condition, _ in self._conditions:
        if not condition:
          return False
      return True

    def enterAssignment(self, ctx):
      if not self._match: return
      if ctx.lvalue().Identifier().getText() == "sources":
        if self._flatten_conditions():
          self._extract_sources(ctx)

    def enterStatement(self, ctx):
      self._depth += 1

    def exitStatement(self, ctx):
      self._depth -= 1

    def get_sources(self):
      seen = set()
      result = []
      ## Deduplicate list while maintaining ordering. needed for js2c files
      for s in self._sources:
        if not s in seen:
          result.append(s)
          seen.add(s)
      return result

def parseSources(tree, target, arch = None, only_cc_files = True):
  listener = V8GnListener(target, arch, only_cc_files)
  ParseTreeWalker().walk(listener, tree)
  return listener.get_sources()

class V8VariableListener(gnListener):
    def __init__(self, variableName):
        super(gnListener, self).__init__()
        self._sources = []
        self._variable = variableName

    def enterAssignment(self, ctx):
      if ctx.lvalue().Identifier().getText() == self._variable:
        op = ctx.AssignOp().getText()
        srcs = map(_expr_to_str, ctx.expr().unaryexpr().primaryexpr().exprlist().expr())
        if DBG: print self._variable, ':', len(srcs)
        if op == '=':
          if self._sources:
            print "WARN: override", self._variable
          self._sources = srcs
        elif op == '+=':
          self._sources.extend(srcs)

    def get_sources(self):
      return self._sources

def parseTorqueSources(tree):
  listener = V8VariableListener("torque_files")
  ParseTreeWalker().walk(listener, tree)
  return listener.get_sources()

def getV8BaseSourceFunc(tree):
    intl_files = set([
      "src/builtins/builtins-intl.cc",
      "src/objects/intl-objects.cc",
      "src/objects/intl-objects.h",
      "src/objects/js-break-iterator-inl.h",
      "src/objects/js-break-iterator.cc",
      "src/objects/js-break-iterator.h",
      "src/objects/js-collator-inl.h",
      "src/objects/js-collator.cc",
      "src/objects/js-collator.h",
      "src/objects/js-date-time-format-inl.h",
      "src/objects/js-date-time-format.cc",
      "src/objects/js-date-time-format.h",
      "src/objects/js-display-names-inl.h",
      "src/objects/js-display-names.cc",
      "src/objects/js-display-names.h",
      "src/objects/js-list-format-inl.h",
      "src/objects/js-list-format.cc",
      "src/objects/js-list-format.h",
      "src/objects/js-locale-inl.h",
      "src/objects/js-locale.cc",
      "src/objects/js-locale.h",
      "src/objects/js-number-format-inl.h",
      "src/objects/js-number-format.cc",
      "src/objects/js-number-format.h",
      "src/objects/js-plural-rules-inl.h",
      "src/objects/js-plural-rules.cc",
      "src/objects/js-plural-rules.h",
      "src/objects/js-relative-time-format-inl.h",
      "src/objects/js-relative-time-format.cc",
      "src/objects/js-relative-time-format.h",
      "src/objects/js-segment-iterator-inl.h",
      "src/objects/js-segment-iterator.cc",
      "src/objects/js-segment-iterator.h",
      "src/objects/js-segmenter-inl.h",
      "src/objects/js-segmenter.cc",
      "src/objects/js-segmenter.h",
      "src/objects/js-segments-inl.h",
      "src/objects/js-segments.cc",
      "src/objects/js-segments.h",
      "src/runtime/runtime-intl.cc",
      "src/strings/char-predicates.cc",
      "src/builtins/builtins-intl-gen.cc",
    ])

    listener = V8VariableListener("v8_compiler_sources")
    ParseTreeWalker().walk(listener, tree)
    v8_compiler_sources = listener.get_sources()
    v8_compiler_sources = [x for x in v8_compiler_sources if x.endswith('.cc')]
    def getSourceFunc(arch):
      return set(parseSources(tree, "v8_base_without_compiler", arch) + v8_compiler_sources) \
          - intl_files
    return getSourceFunc

def GenerateMakefiles():
    f = FileStream(os.path.join(os.getcwd(), './BUILD.gn'))
    lexer = gnLexer(f)
    stream = CommonTokenStream(lexer)
    parser = gnParser(stream)
    tree = parser.r()

    _writeLibBaseBP(parseSources(tree, "v8_libbase"))
    _writeBP('Android.libplatform.bp', 'v8_libplatform', parseSources(tree, "v8_libplatform"))
    _writeBP('Android.libsampler.bp', 'v8_libsampler', parseSources(tree, "v8_libsampler"))
    _writeV8BaseBP(getV8BaseSourceFunc(tree))
    v8_init_sources = parseSources(tree, "v8_initializers")
    v8_init_sources.remove("src/builtins/builtins-intl-gen.cc")
    _writeFileGroupBP('Android.initializers.bp', 'v8_initializers', v8_init_sources)
    _writeTorqueBP(parseTorqueSources(tree))

if __name__ == '__main__':
  GenerateMakefiles()
