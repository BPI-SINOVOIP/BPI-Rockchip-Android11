gnome = import('gnome')

ignore_headers = [
	'gen6_render.h',
	'gen7_media.h',
	'gen7_render.h',
	'gen8_media.h',
	'gen8_render.h',
	'gpgpu_fill.h',
	'i830_reg.h',
	'i915_3d.h',
	'i915_pciids.h',
	'i915_reg.h',
	'igt_edid_template.h',
	'intel_reg.h',
	'debug.h',
	'instdone.h',
	'media_fill.h',
	'rendercopy.h',
	'media_spin.h',
	'media_fill_gen9.h',
	'gen9_render.h',
	'version.h',
]

test_groups = [
	'amdgpu',
	'chamelium',
	'core',
	'debugfs',
	'drm',
	'gem',
	'gen3',
	'gen7',
	'gvt',
	'i915',
	'kms',
	'meta',
	'perf',
	'pm',
	'prime',
	'sw_sync',
	'testdisplay',
	'tools',
	'vgem',
]

gen_description = find_program('generate_description_xml.py')
gen_programs = find_program('generate_programs_xml.sh')

generated_docs = []

configure_file(input: 'version.xml.in',
	       output: 'version.xml',
	       install: false, configuration: config)

foreach group : test_groups
	programs_xml = 'igt_test_programs_' + group + '_programs.xml'
	generated_docs += custom_target(programs_xml,
		      output : programs_xml,
		      command : [ gen_programs, '@OUTPUT@', group, test_list_target ])

	description_xml = 'igt_test_programs_' + group + '_description.xml'
	generated_docs += custom_target(description_xml,
		      output : description_xml,
		      depends : test_executables,
		      command : [ gen_description, '@OUTPUT@', group, test_list_target ])
endforeach

gnome.gtkdoc('igt-gpu-tools',
	     content_files : ['igt_test_programs.xml'] + generated_docs,
	     dependencies : lib_igt,
	     install : true,
	     main_xml : 'igt-gpu-tools-docs.xml',
	     scan_args : '--rebuild-sections',
	     mkdb_args : '--output-format=xml',
	     ignore_headers : ignore_headers,
	     src_dir : inc_for_gtkdoc)
