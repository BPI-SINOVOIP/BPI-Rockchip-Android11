lib_brw_src = [
	'brw_context.c',
	'brw_disasm.c',
	'brw_eu.c',
	'brw_eu_compact.c',
	'brw_eu_debug.c',
	'brw_eu_emit.c',
	'brw_eu_util.c',
	'gen8_disasm.c',
	'gen8_instruction.c',
	'ralloc.c',
]

assembler_warnflags = [
	'-Wno-declaration-after-statement',
	'-Wno-discarded-qualifiers',
	'-Wno-missing-format-attribute',
	'-Wno-old-style-definition',
	'-Wno-redundant-decls',
	'-Wno-shadow',
]
assembler_args = []
foreach flag : assembler_warnflags
	if cc.has_argument(flag)
		assembler_args += flag
	endif
endforeach

lib_brw = static_library('brw', lib_brw_src,
			 c_args : assembler_args,
			 dependencies : igt_deps)

flex = find_program('flex')
bison = find_program('bison')

lgen = generator(flex,
		output : '@BASENAME@.c',
		arguments : ['-i', '-o', '@OUTPUT@', '@INPUT@'])

lfiles = lgen.process('lex.l')

pgen = generator(bison,
		output : ['@BASENAME@.c', '@BASENAME@.h'],
		arguments : ['@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@'])

pfiles = pgen.process('gram.y')

executable('intel-gen4asm', 'main.c', lfiles, pfiles,
	   c_args : assembler_args,
	   link_with : lib_brw, install : true)

executable('intel-gen4disasm', 'disasm-main.c',
	   c_args : assembler_args,
	   link_with : lib_brw, install : true)

conf_data = configuration_data()
conf_data.set('prefix', prefix)
conf_data.set('exec_prefix', '${prefix}')
conf_data.set('libdir', join_paths('${prefix}', libdir))
conf_data.set('includedir', join_paths('${prefix}', includedir))
conf_data.set('VERSION', meson.project_version())
configure_file(input : 'intel-gen4asm.pc.in', output : 'intel-gen4asm.pc',
	       configuration : conf_data, install_dir : pkgconfigdir)

gen4asm_testcases = [
	'test/mov',
	'test/frc',
	'test/regtype',
	'test/rndd',
	'test/rndu',
	'test/rnde',
	'test/rnde-intsrc',
	'test/rndz',
	'test/lzd',
	'test/not',
	'test/immediate',
]

# Those tests were already failing when the assembler was imported from
# the intel-gen4asm git repository:
#   http://cgit.freedesktop.org/xorg/app/intel-gen4asm/
# We disable them "for now" as a workaround to be able to release i-g-t
gen4asm_testcases_broken = [
	'test/declare',
	'test/jmpi',
	'test/if',
	'test/iff',
	'test/while',
	'test/else',
	'test/break',
	'test/cont',
	'test/halt',
	'test/wait',
	'test/endif',
]

test_runner = find_program('test/run-test.sh')
foreach testcase : gen4asm_testcases
	test('assembler: ' + testcase, test_runner,
			args : testcase,
			env : [ 'srcdir=' + meson.current_source_dir(),
				'top_builddir=' + meson.current_build_dir()])
endforeach
