
amdgpu_progs = []
amdgpu_deps = test_deps
if libdrm_amdgpu.found()
	amdgpu_progs += [ 'amd_abm',
			  'amd_basic',
			  'amd_bypass',
			  'amd_color',
			  'amd_cs_nop',
			  'amd_prime',
			]
	amdgpu_deps += libdrm_amdgpu
endif

foreach prog : amdgpu_progs
	test_executables += executable(prog, prog + '.c',
				       dependencies : amdgpu_deps,
				       install_dir : amdgpudir,
				       install_rpath : amdgpudir_rpathdir,
				       install : true)
	test_list += join_paths('amdgpu', prog)
endforeach
