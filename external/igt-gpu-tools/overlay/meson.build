build_overlay = get_option('overlay')
overlay_backends = get_option('overlay_backends')

gpu_overlay_src = [
	'chart.c',
	'config.c',
	'cpu-top.c',
	'debugfs.c',
	'gem-interrupts.c',
	'gem-objects.c',
	'gpu-top.c',
	'gpu-perf.c',
	'gpu-freq.c',
	'overlay.c',
	'power.c',
	'rc6.c',
]

xv_backend_required = false
xlib_backend_required = false
build_xv_backend = overlay_backends.contains('xv') or overlay_backends.contains('auto')
build_xlib_backend = overlay_backends.contains('x') or overlay_backends.contains('auto')
if build_overlay.enabled()
	xv_backend_required = overlay_backends.contains('xv')
	xlib_backend_required = overlay_backends.contains('x')
endif

xv = dependency('xv', required : xv_backend_required)
x11 = dependency('x11', required : xv_backend_required)
xext = dependency('xext', required : xv_backend_required)
dri2proto = dependency('dri2proto',
		       version : '>= 2.6',
		       required : xv_backend_required or xlib_backend_required)
cairo_xlib = dependency('cairo-xlib', required : xlib_backend_required)
xrandr = dependency('xrandr', version : '>=1.3', required : build_overlay)

gpu_overlay_deps = [ realtime, math, cairo, pciaccess, libdrm,
	libdrm_intel, lib_igt_perf ]

both_x11_src = ''

with_xv_backend = false
with_xlib_backend = false
backends_strings = []

gpu_overlay_cflags = []
if build_xv_backend and xv.found() and x11.found() and xext.found() and dri2proto.found()
	both_x11_src = 'x11/position.c'
	gpu_overlay_src += [
		'x11/dri2.c',
		'x11/dri2.h',
		'x11/rgb2yuv.c',
		'x11/rgb2yuv.h',
		'x11/x11-overlay.c',
	]
	gpu_overlay_deps += [ xv, x11, xext, dri2proto ]
	gpu_overlay_cflags += [ '-DHAVE_OVERLAY_XVLIB' ]
	with_xv_backend = true
	backends_strings += 'Xv'
endif

if build_xlib_backend and cairo_xlib.found() and dri2proto.found()
	both_x11_src = 'x11/position.c'
	gpu_overlay_src += 'x11/x11-window.c'
	gpu_overlay_deps += [ cairo_xlib, dri2proto ]
	gpu_overlay_cflags += [ '-DHAVE_OVERLAY_XLIB' ]
	with_xlib_backend = true
	backends_strings += 'X'
endif

gpu_overlay_src += both_x11_src

gpu_overlay_src += 'kms/kms-overlay.c'

leg = find_program('leg', required : build_overlay)
if leg.found()
	leg_file = custom_target('tracepoint_format',
		output: 'tracepoint_format.h',
		input: 'tracepoint_format.leg',
		command: [leg, '-P', '-o', '@OUTPUT@', '@INPUT@'])
	gpu_overlay_src += leg_file
else
	message('WARNING: leg command not found, disabling overlay; try : apt-get install peg')
endif

if not build_overlay.disabled() and ['x86', 'x86_64'].contains(host_machine.cpu_family()) and libdrm_intel.found() and leg.found() and xrandr.found() and cairo.found() and (with_xlib_backend or with_xv_backend)
	executable('intel-gpu-overlay', gpu_overlay_src,
			include_directories : inc,
			c_args : gpu_overlay_cflags,
			dependencies : gpu_overlay_deps,
			install : true)
	build_info += 'Build overlay: true'
	build_info += 'Overlay backends: ' + ','.join(backends_strings)
else
	if build_overlay.enabled()
		error('Cannot build overlay due to missing dependencies')
	endif
	build_info += 'Build overlay: false'
endif
