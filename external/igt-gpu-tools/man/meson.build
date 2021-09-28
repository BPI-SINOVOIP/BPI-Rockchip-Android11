build_man = get_option('man')

manpages = [
	'intel_aubdump',
	'intel_audio_dump',
	'intel_bios_dumper',
	'intel_error_decode',
	'intel_gpu_frequency',
	'intel_gpu_top',
	'intel_gtt',
	'intel_infoframes',
	'intel_lid',
	'intel_panel_fitter',
	'intel_reg',
	'intel_stepping',
	'intel_upload_blit_large',
	'intel_upload_blit_large_gtt',
	'intel_upload_blit_large_map',
	'intel_upload_blit_small',
	'intel_vbt_decode',
]

defs_rst = configure_file(input : 'defs.rst.in',
		output : 'defs.rst',
		configuration : config)

rst2man = find_program('rst2man-3', 'rst2man', required : build_man)
rst2man_script = find_program('rst2man.sh')

if rst2man.found()
	foreach manpage : manpages
		custom_target(manpage + '.1',
				build_by_default : true,
				command : [ rst2man_script, rst2man, '@INPUT@', '@OUTPUT@' ],
				depend_files : [ defs_rst ],
				input: manpage + '.rst',
				output : manpage + '.1.gz',
				install : true,
				install_dir : join_paths(mandir, 'man1'))
	endforeach
endif

build_info += 'Build man pages: @0@'.format(rst2man.found())
