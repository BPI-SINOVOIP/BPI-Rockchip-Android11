lib_sources = [
	'drmtest.c',
	'i915/gem_context.c',
	'i915/gem_engine_topology.c',
	'i915/gem_scheduler.c',
	'i915/gem_submission.c',
	'i915/gem_ring.c',
	'i915/gem_mman.c',
	'i915/gem_vm.c',
	'igt_color_encoding.c',
	'igt_debugfs.c',
	'igt_device.c',
	'igt_aux.c',
	'igt_gpu_power.c',
	'igt_gt.c',
	'igt_gvt.c',
	'igt_halffloat.c',
	'igt_matrix.c',
	'igt_perf.c',
	'igt_primes.c',
	'igt_rand.c',
	'igt_stats.c',
	'igt_syncobj.c',
	'igt_sysfs.c',
	'igt_sysrq.c',
	'igt_vgem.c',
	'igt_x86.c',
	'instdone.c',
	'intel_batchbuffer.c',
	'intel_chipset.c',
	'intel_device_info.c',
	'intel_os.c',
	'intel_mmio.c',
	'ioctl_wrappers.c',
	'media_spin.c',
	'media_fill.c',
	'gpgpu_fill.c',
	'gpu_cmds.c',
	'rendercopy_i915.c',
	'rendercopy_i830.c',
	'rendercopy_gen4.c',
	'rendercopy_gen6.c',
	'rendercopy_gen7.c',
	'rendercopy_gen8.c',
	'rendercopy_gen9.c',
	'sw_sync.c',
	'intel_reg_map.c',
	'intel_iosf.c',
	'igt_kms.c',
	'igt_fb.c',
	'igt_core.c',
	'igt_draw.c',
	'igt_pm.c',
	'igt_dummyload.c',
	'uwildmat/uwildmat.c',
	'igt_kmod.c',
	'igt_panfrost.c',
	'igt_v3d.c',
	'igt_vc4.c',
	'igt_psr.c',
	'igt_amd.c',
	'igt_edid.c',
	'igt_eld.c',
	'igt_infoframe.c',
]

lib_deps = [
	cairo,
	glib,
	libdrm,
	libdw,
	libkmod,
	libprocps,
	libudev,
	math,
	pciaccess,
	pixman,
	pthreads,
	realtime,
	zlib
]

if libdrm_intel.found()
	lib_deps += libdrm_intel
else
	lib_sources += 'stubs/drm/intel_bufmgr.c'
	inc = [ inc, include_directories('stubs/drm') ]
endif

if libunwind.found()
	lib_deps += libunwind
else
	inc = [ inc, include_directories('stubs/libunwind') ]
endif

if valgrind.found()
	lib_deps += valgrind
endif

if gsl.found()
	lib_deps += gsl
	lib_sources += [ 'igt_frame.c', 'igt_audio.c' ]
endif

if alsa.found()
	lib_deps += alsa
	lib_sources += 'igt_alsa.c'
endif

if chamelium.found()
	lib_deps += chamelium
	lib_sources += 'igt_chamelium.c'
	lib_sources += 'igt_chamelium_stream.c'
endif

srcdir = join_paths(meson.source_root(), 'tests')

lib_version = vcs_tag(input : 'version.h.in', output : 'version.h',
		      fallback : 'NO-GIT',
		      command : [ 'git', 'log', '-n1', '--pretty=format:g%h' ] )

lib_intermediates = []
foreach f: lib_sources
    name = f.underscorify(f)
    lib = static_library('igt-' + name,
	[ f, lib_version ],
	include_directories: inc,
	dependencies : lib_deps,
	c_args : [
	    '-DIGT_DATADIR="@0@"'.format(join_paths(prefix, datadir)),
	    '-DIGT_SRCDIR="@0@"'.format(srcdir),
	    '-DIGT_LOG_DOMAIN="@0@"'.format(f.split('.')[0]),
	])

    lib_intermediates += lib
endforeach

lib_igt_build = shared_library('igt',
    ['dummy.c'],
    link_whole: lib_intermediates,
    dependencies: lib_deps,
    install : true,
    soversion : '0',
)

lib_igt = declare_dependency(link_with : lib_igt_build,
			    include_directories : inc)

igt_deps = [ lib_igt ] + lib_deps

lin_igt_chipset_build = static_library('igt_chipset',
                                       ['intel_chipset.c',
                                        'intel_device_info.c'],
                                       include_directories : inc)

lib_igt_chipset = declare_dependency(link_with : lin_igt_chipset_build,
                                     include_directories : inc)

lib_igt_perf_build = static_library('igt_perf',
	['igt_perf.c'],
	include_directories : inc)

lib_igt_perf = declare_dependency(link_with : lib_igt_perf_build,
				  include_directories : inc)

subdir('tests')
