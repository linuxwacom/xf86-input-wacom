project('xf86-input-wacom', 'c',
	version : '1.2.1', # bump version in configure.ac
	license : 'MIT/Expat',
	default_options : [ 'c_std=gnu99', 'warning_level=2' ],
	meson_version : '>= 0.51.0')

wacom_version = meson.project_version().split('.')

dir_man1	= get_option('prefix') / get_option('mandir') / 'man1'
dir_man4	= get_option('prefix') / get_option('mandir') / 'man4'
dir_bin 	= get_option('prefix') / get_option('bindir')
dir_x11_config	= get_option('prefix') / get_option('datadir') / 'X11' / 'xorg.conf.d'
dir_pkgconf	= get_option('prefix') / get_option('libdir') / 'pkgconfig'
dir_udev	= get_option('prefix') / 'lib' / 'udev'
dir_systemd	= get_option('prefix') / 'lib' / 'systemd'
dir_src_test	= 'test'
dir_src		= 'src'
dir_include	= 'include'


# Compiler setup
cc = meson.get_compiler('c')

cflags = [
	# '-Wall', handled through meson's warning_level option
	# '-Wextra', handled through meson's warning_level option
	'-Wno-unused-parameter',
	'-Wmissing-prototypes',
	'-Wstrict-prototypes',
	'-Wlogical-op',
	'-Wpointer-arith',
	'-Wuninitialized',
	'-Winit-self',
	'-Wstrict-prototypes',
	'-Wimplicit-fallthrough',
	'-Wredundant-decls',
	'-Wincompatible-pointer-types',
	'-Wformat=2',
	'-Wformat-overflow=2',
	'-Wformat-signedness',
	'-Wformat-truncation=2',
	'-Wmissing-declarations',
	'-Wshift-overflow=2',
	'-Wstrict-overflow=2',
	'-Wswitch-enum',
]
add_project_arguments(cc.get_supported_arguments(cflags), language : 'c')
add_project_arguments('-D_GNU_SOURCE', language : 'c')

build_xsetwacom = get_option('xsetwacom')

# Dependencies
pkgconfig = import('pkgconfig')
dep_xserver = dependency('xorg-server', version: '>= 1.13.0')
if build_xsetwacom
	dep_protos = []
	protos = ['xproto', 'xext', 'kbproto', 'inputproto', 'randrproto']
	foreach proto : protos
		dep_protos += [dependency(proto)]
	endforeach

	xlibs = ['x11', 'xi', 'xrandr', 'xinerama']
	dep_xlibs = []
	foreach xlib : xlibs
		dep_xlibs += [dependency(xlib)]
	endforeach
endif
dep_libudev = dependency('libudev')
dep_m = cc.find_library('m')

dir_wacom_headers = get_option('sdkdir')
if dir_wacom_headers == ''
	dir_wacom_headers = dep_xserver.get_variable(pkgconfig: 'sdkdir')
endif

dir_xorg_modules = get_option('xorg-module-dir')
if dir_xorg_modules == ''
	dir_xorg_modules = dep_xserver.get_variable(pkgconfig: 'moduledir') / 'input'
endif

dir_xorg_conf = get_option('xorg-conf-dir')
if dir_xorg_conf == ''
	dir_xorg_conf = dir_x11_config
endif

dir_udev_rules = get_option('udev-rules-dir')
if dir_udev_rules == ''
	dir_udev_rules = dir_udev / 'rules.d'
endif

dir_systemd_unit = get_option('systemd-unit-dir')
if dir_udev_rules == ''
	dir_systemd_unit = dir_systemd / 'systemd'
endif


# config-ver.h
config_ver_h = vcs_tag(command : ['git', 'describe'],
			fallback : meson.project_version(),
			input : 'config-ver.h.in',
			output :'config-ver.h',
			replace_string: '@GITVERSION@')


# config.h
config_h = configuration_data()
config_h.set('_GNU_SOURCE', 1)
config_h.set_quoted('PACKAGE_VERSION', meson.project_version())
config_h.set('PACKAGE_VERSION_MAJOR', wacom_version[0])
config_h.set('PACKAGE_VERSION_MINOR', wacom_version[1])
config_h.set('PACKAGE_VERSION_PATCHLEVEL', wacom_version[2])

# shortcut to avoid linking to _Xasprintf if xorg-server isn't included
if cc.has_function('asprintf',
	prefix: '#define _GNU_SOURCE',
	args: ['-Wno-error=format'])
	config_h.set10('HAVE_ASPRINTF', true)
endif

if get_option('debug-messages')
	config_h.set10('DEBUG', true)
endif

if get_option('fuzzinterface')
	config_h.set10('BUILD_FUZZINTERFACE', true)
endif


# Driver
src_wacom_core = [
	config_ver_h,
	'src/WacomInterface.h',
	'src/wcmCommon.c',
	'src/wcmConfig.c',
	'src/wcmFilter.c',
	'src/wcmFilter.h',
	'src/wcmPressureCurve.c',
	'src/wcmTouchFilter.c',
	'src/wcmTouchFilter.h',
	'src/wcmUSB.c',
	'src/wcmValidateDevice.c',
	'src/xf86WacomDefs.h',
	'src/xf86Wacom.h',
	'src/wcmUSB.c',
]

src_wacom = src_wacom_core + [
	'src/x11/xf86WacomProperties.c',
	'src/x11/xf86Wacom.c',
]

shared_module(
	'wacom_drv',
	src_wacom,
	include_directories: [dir_src, dir_include],
	dependencies: [dep_xserver, dep_m],
	name_prefix: '', # we want wacomdrv.so, not libwacomdrv.so
	install_dir: dir_xorg_modules,
	install: true,
	# Note: xorg-xserver.pc always appends -fvisibility=hidden so
	# this correct but superfluous
	gnu_symbol_visibility: 'hidden',
)

conf_pkgconf = configuration_data()
conf_pkgconf.set('PACKAGE_VERSION', meson.project_version())
conf_pkgconf.set('sdkdir', dir_wacom_headers)

configure_file(
	input: 'xorg-wacom.pc.in',
	output: 'xorg-wacom.pc',
	configuration: conf_pkgconf,
	install_dir: dir_pkgconf,
)

install_headers(
	'include/Xwacom.h',
	'include/wacom-properties.h',
	'include/isdv4.h',
	'include/wacom-util.h',
	install_dir: dir_wacom_headers
)

# GWacom library
# This is not a stable library interface

dep_glib = dependency('glib-2.0', required: get_option('wacom-gobject'))
dep_gobject = dependency('gobject-2.0', required: get_option('wacom-gobject'))
dep_gio = dependency('gio-2.0', required: get_option('wacom-gobject'))
dep_gio_unix = dependency('gio-unix-2.0', required: get_option('wacom-gobject'))
dep_object_ir = dependency('gobject-introspection-1.0', required: get_option('wacom-gobject'))
dep_libevdev = dependency('libevdev', required: get_option('wacom-gobject'))

build_gwacom = dep_glib.found() and dep_gobject.found() and dep_gio.found() and dep_gio_unix.found() and dep_object_ir.found() and dep_libevdev.found()

if build_gwacom
	gnome = import('gnome')

	src_gwacom = [
		'src/gwacom/wacom-driver.h',
		'src/gwacom/wacom-driver.c',
		'src/gwacom/wacom-device.h',
		'src/gwacom/wacom-device.c',
	]

	src_libgwacom = src_wacom_core + src_gwacom

	deps_gwacom = [
		dep_xserver,
		dep_m,
		dep_glib,
		dep_gobject,
		dep_gio,
		dep_gio_unix,
		dep_libevdev,
	]

	lib_gwacom = shared_library(
		'gwacom',
		src_libgwacom + src_wacom_core,
		include_directories: [dir_src, dir_include],
		dependencies: deps_gwacom,
		install: false,
		# Note: xorg-xserver.pc always appends -fvisibility=hidden so
		# this is the only way to force default visibility
		c_args: ['-fvisibility=default'],
	)

	dep_gwacom = declare_dependency(
		link_with: lib_gwacom,
		dependencies: deps_gwacom,
		include_directories: ['src/gwacom/'],
	)

	# GIR bindings are not installed, use
	# export GI_TYPELIB_PATH="$PWD/builddir:$LD_LIBRARY_PATH"
	# export LD_LIBRARY_PATH="$PWD/builddir:$LD_LIBRARY_PATH"
	dep_gir = gnome.generate_gir(lib_gwacom,
		sources: src_gwacom,
		dependencies: [deps_gwacom],
		namespace: 'wacom',
		nsversion: '1.0',
		identifier_prefix: 'Wacom',
		symbol_prefix: 'wacom',
		includes: 'GObject-2.0',
		install: false,
	)

	# A custom target that forces the GIR bindings to be built. Usually
	# those get built on install but we don't install ours.
	custom_target('force GIR build',
		build_by_default: true,
		command: ['echo'],
		output: 'force-gir-build.dummy',
		capture: true,
		depends: [dep_gir])

	executable('wacom-record',
		'tools/wacom-record.c',
		config_ver_h,
		dependencies: [dep_libudev, dep_glib, dep_gwacom],
		install: false,
	)
endif

# Tools
if get_option('serial-device-support')
	src_shared = [
		'tools/tools-shared.h',
		'tools/tools-shared.c',
	]

	executable(
		'isdv4-serial-debugger',
		'tools/isdv4-serial-debugger.c', src_shared,
		dependencies: [dep_libudev],
		include_directories: [dir_include],
		install: true,
	)
	executable(
		'isdv4-serial-inputattach',
		'tools/isdv4-serial-inputattach.c', src_shared,
		dependencies: [dep_libudev],
		include_directories: [dir_include],
		install: true,
	)
endif

if build_xsetwacom
	xsetwacom_deps = [dep_xlibs, dep_protos, dep_m]
	src_xsetwacom = [
		config_ver_h,
		'tools/xsetwacom.c',
	]
	executable(
		'xsetwacom',
		src_xsetwacom,
		dependencies: xsetwacom_deps,
		include_directories: [dir_include],
		install: true,
	)
endif

executable(
	'pressurecurve',
	['tools/pressurecurve.c',
	 'src/wcmPressureCurve.c'],
	include_directories: [dir_src],
	install: false)

# Man pages
config_man = configuration_data()
config_man.set('VERSION', '@0@ @1@'.format(meson.project_name(), meson.project_version()))

configure_file(
	input: 'man/wacom.man',
	output: 'wacom.4',
	configuration: config_man,
	install_dir: dir_man4
)
configure_file(
	input: 'man/xsetwacom.man',
	output: 'xsetwacom.1',
	configuration: config_man,
	install_dir: dir_man1,
)

# Config files
install_data('conf/70-wacom.conf', install_dir: dir_xorg_conf)
if get_option('serial-device-support')
	install_data('conf/wacom.rules', install_dir: dir_udev_rules)
	conf_service = configuration_data()
	conf_service.set('BIN_PREFIX', dir_bin)
	configure_file(
		input: 'conf/wacom-inputattach@.service.in',
		output: 'wacom-inputattach@.service',
		configuration: conf_service,
		install_dir: dir_systemd_unit,
	)
endif

# Tests
build_unittests = not get_option('unittests').disabled()

if build_unittests
	if build_xsetwacom
		xsetwacom_test = executable(
			'xsetwacom-test',
			src_xsetwacom + ['test/wacom-test-suite.c', 'test/wacom-test-suite.h'],
			c_args: ['-DENABLE_TESTS', '-Wno-unused-function'],
			dependencies: xsetwacom_deps,
			include_directories: [dir_include, dir_src, dir_src_test],
		)
		test('xsetwacom-tests', xsetwacom_test)
	endif

	# Rebuild the driver with -DENABLE_TESTS to enable all the various
	# local tests in the files. This is built as a module (like the
	# driver) so we don't need to resolve any symbols until runtime.
	# Look at wacom-test-suite.(c|h) for the magic sauce.
	wacom_drv_test = shared_module(
		'wacom_drv_test',
		src_wacom + ['test/wacom-test-suite.c', 'test/wacom-test-suite.h'],
		include_directories: [dir_src, dir_include, dir_src_test],
		dependencies: [dep_xserver, dep_m],
		name_prefix: '', # we want wacom_drv_test.so, not libwacom_drv_test.so
		install: false,
		# Note: xorg-xserver.pc always appends -fvisibility=hidden so
		# this is the only way to force default visibility
		c_args: ['-DENABLE_TESTS', '-fvisibility=default'],
	)
	dep_dl = cc.find_library('dl')
	# wacom-tests.c is just a stub to load the above driver and run the
	# entry point.
	test('wacom-tests',
		executable(
		'wacom-tests',
		'test/wacom-tests.c',
		dependencies: [dep_dl],
		install: false),
		env: [
			'LD_LIBRARY_PATH=@0@'.format(meson.current_build_dir()),
		]
	)

	devenv = environment()
	devenv.set('LD_LIBRARY_PATH', meson.current_build_dir())
	devenv.set('GI_TYPELIB_PATH', meson.current_build_dir())

	# pytest doesn't like asan or ubsan
	if build_gwacom and get_option('b_sanitize') == 'none'
		pymod = import('python')
		pymod.find_installation(
			'python3',
			modules: ['libevdev', 'pytest', 'yaml', 'attr', 'gi']
		)

		pytest = find_program('pytest-3', 'pytest')
		test('pytest',
		     pytest,
		     args: ['--verbose', '--log-level=DEBUG'],
		     env: devenv,
		     workdir: meson.current_source_dir(),
		)
	endif

	if meson.version().version_compare('>= 0.58.0')
		meson.add_devenv(devenv)
	endif
endif


# Now generate config.h
configure_file(output: 'config.h', configuration: config_h)
