project('sfwbar','c')
gtk3 = dependency('gtk+-3.0')
glib = dependency('glib-2.0')
json  = dependency('json-c')
glsh = dependency('gtk-layer-shell-0')
wayl = dependency('wayland-client')
wayp = dependency('wayland-protocols', version: '>=1.17')

wayland_scanner = find_program('wayland-scanner')
wprotodir = wayp.get_pkgconfig_variable('pkgdatadir')

ftop_c = custom_target('wlr-foreign-toplevel-management-unstable-v1.c',
    output: 'wlr-foreign-toplevel-management-unstable-v1.c',
    input: 'protocols/wlr-foreign-toplevel-management-unstable-v1.xml',
    command: [ wayland_scanner, 'public-code', '@INPUT@', '@OUTPUT@'] )

ftop_h = custom_target('wlr-foreign-toplevel-management-unstable-v1.h',
    output: 'wlr-foreign-toplevel-management-unstable-v1.h',
    input: 'protocols/wlr-foreign-toplevel-management-unstable-v1.xml',
    command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'] )

xout_c = custom_target('xdg-output-unstable-v1.c',
    output: 'xdg-output-unstable-v1.c',
    input: 'protocols/xdg-output-unstable-v1.xml',
    command: [ wayland_scanner, 'public-code', '@INPUT@', '@OUTPUT@'] )

xout_h = custom_target('xdg-output-unstable-v1.h',
    output: 'xdg-output-unstable-v1.h',
    input: 'protocols/xdg-output-unstable-v1.xml',
    command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'] )

idle_c = custom_target('idle-inhibit-unstable-v1.c',
    output: 'idle-inhibit-unstable-v1.c',
    input: wprotodir / 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml',
    command: [ wayland_scanner, 'public-code', '@INPUT@', '@OUTPUT@'] )

idle_h = custom_target('idle-inhibit-unstable-v1.h',
    output: 'idle-inhibit-unstable-v1.h',
    input: wprotodir / 'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml',
    command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'] )

src = ['src/sfwbar.c',
    'src/client.c',
    'src/sway_ipc.c',
    'src/placement.c',
    'src/layout.c',
    'src/taskbar.c',
    'src/switcher.c',
    'src/scanner.c',
    'src/wintree.c',
    'src/expr.c',
    'src/jpath.c',
    'src/misc.c',
    'src/mpd.c',
    'src/pager.c',
    'src/scaleimage.c',
    'src/flowgrid.c',
    'src/wayland.c',
    'src/sni.c',
    'src/action.c',
    'src/config.c',
    ftop_c,
    ftop_h,
    idle_c,
    idle_h,
    xout_c,
    xout_h ]
dep = [gtk3, glib, glsh, wayl, json ]


executable ('sfwbar', sources: src, c_args: ['-DGLIB_DISABLE_DEPRECATION_WARNINGS','-DGDK_DISABLE_DEPRECATED','-DGTK_DISABLE_DEPRECATED'], dependencies: dep, install: true)

install_man('doc/sfwbar.1')
install_subdir('config', install_dir: get_option('datadir') / 'sfwbar', strip_directory : true )
install_subdir('icons/weather', install_dir: get_option('datadir') / 'sfwbar/icons' )
install_subdir('icons/misc', install_dir: get_option('datadir') / 'sfwbar/icons' )
