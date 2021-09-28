intel_null_state_gen_src = [
	'intel_batchbuffer.c',
	'intel_renderstate_gen6.c',
	'intel_renderstate_gen7.c',
	'intel_renderstate_gen8.c',
	'intel_renderstate_gen9.c',
	'intel_null_state_gen.c',
]

intel_null_state_gen = executable('intel_null_state_gen',
		intel_null_state_gen_src, dependencies : igt_deps,
		install : false)

# FIXME actual null_state generation. We probably want to push the the copyright
# header generation into intel_null_state_gen for simplicity
