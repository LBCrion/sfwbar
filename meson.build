project('sfwbar','c')
gtk3 = dependency('gtk+-3.0')
glib = dependency('glib-2.0')
json  = dependency('json-c')
glsh = dependency('gtk-layer-shell-0')
wayl = dependency('wayland-client')

wayland_scanner = find_program('wayland-scanner')

ftop_c = custom_target('wlr-foreign-toplevel-management-unstable-v1.c',
    output: 'wlr-foreign-toplevel-management-unstable-v1.c',
    input: 'protocols/wlr-foreign-toplevel-management-unstable-v1.xml',
    command: [ wayland_scanner, 'public-code', '@INPUT@', '@OUTPUT@'] )

ftop_h = custom_target('wlr-foreign-toplevel-management-unstable-v1.h',
    output: 'wlr-foreign-toplevel-management-unstable-v1.h',
    input: 'protocols/wlr-foreign-toplevel-management-unstable-v1.xml',
    command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'] )

src = ['src/sfwbar.c',
    'src/sway_ipc.c',
    'src/placement.c',
    'src/layout.c',
    'src/taskbar.c',
    'src/switcher.c',
    'src/scanner.c',
    'src/wintree.c',
    'src/expr.c',
    'src/misc.c',
    'src/pager.c',
    'src/scaleimage.c',
    'src/clampgrid.c',
    'src/wayland.c',
    'src/sni.c',
    'src/config.c',
    ftop_c,
    ftop_h ]
dep = [gtk3, glib, glsh, wayl, json ]


executable ('sfwbar', sources: src, c_args: ['-DGLIB_DISABLE_DEPRECATION_WARNINGS','-DGDK_DISABLE_DEPRECATED','-DGTK_DISABLE_DEPRECATED'], dependencies: dep, install: true)

install_man('doc/sfwbar.1')
install_subdir('config', install_dir: get_option('datadir') / 'sfwbar', strip_directory : true )
install_subdir('icons/weather', install_dir: get_option('datadir') / 'sfwbar/icons' )
install_subdir('icons/misc', install_dir: get_option('datadir') / 'sfwbar/icons' )


