project('sfwbar','c')
gtk3 = dependency('gtk+-3.0')
glib = dependency('glib-2.0')
gio_unix = dependency('gio-unix-2.0')
gmod = dependency('gmodule-2.0')
json  = dependency('json-c')
glsh = dependency('gtk-layer-shell-0')
wayl = dependency('wayland-client')
wayp = dependency('wayland-protocols', version: '>=1.17')
puls = dependency('libpulse', required: get_option('pulse'))
pulg = dependency('libpulse-mainloop-glib', required: get_option('pulse'))
mpdc = dependency('libmpdclient', required: get_option('mpd'))
xkbr = dependency('xkbregistry', required: get_option('xkb'))

conf_data = configuration_data()
glsh_ver = glsh.version().split('.')
conf_data.set('glsh_major',glsh_ver[0])
conf_data.set('glsh_minor',glsh_ver[1])
conf_data.set('glsh_micro',glsh_ver[2])
conf_data.set('module_dir',
    get_option('prefix') / get_option('libdir') / 'sfwbar')
configure_file(input: 'meson.h.meson', output: 'meson.h',
    configuration: conf_data )

wayland_scanner = find_program('wayland-scanner')
wayland_protodir = wayp.get_pkgconfig_variable('pkgdatadir')
wayland_protos = [
  'protocols/wlr-foreign-toplevel-management-unstable-v1',
  'protocols/xdg-output-unstable-v1',
  wayland_protodir / 'unstable/idle-inhibit/idle-inhibit-unstable-v1'
]
wayland_targets=[]

foreach proto : wayland_protos
  xml  = ''.join([proto,'.xml']) 
  header = ''.join([proto.split('/').get(-1),'.h'])
  cfile = ''.join([proto.split('/').get(-1),'.c'])
  wayland_targets += custom_target(header,output:header,input:xml,
      command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ] )
  wayland_targets += custom_target(cfile,output:cfile,input:xml,
      command: [ wayland_scanner, 'public-code', '@INPUT@', '@OUTPUT@' ] )
endforeach

src = ['src/sfwbar.c',
    'src/bar.c',
    'src/basewidget.c',
    'src/button.c',
    'src/css.c',
    'src/label.c',
    'src/scale.c',
    'src/cchart.c',
    'src/image.c',
    'src/grid.c',
    'src/client.c',
    'src/chart.c',
    'src/flowitem.c',
    'src/hypr_ipc.c',
    'src/taskbaritem.c',
    'src/menu.c',
    'src/sway_ipc.c',
    'src/taskbar.c',
    'src/taskbargroup.c',
    'src/switcher.c',
    'src/switcheritem.c',
    'src/scanner.c',
    'src/wintree.c',
    'src/expr.c',
    'src/exprlib.c',
    'src/jpath.c',
    'src/misc.c',
    'src/module.c',
    'src/mpd.c',
    'src/pager.c',
    'src/pageritem.c',
    'src/popup.c',
    'src/scaleimage.c',
    'src/flowgrid.c',
    'src/wayland.c',
    'src/sni.c',
    'src/sniitem.c',
    'src/snimenu.c',
    'src/trayitem.c',
    'src/tray.c',
    'src/action.c',
    'src/config/base.c',
    'src/config/init.c',
    'src/config/layout.c',
    'src/config/placer.c',
    'src/config/scanner.c',
    'src/config/switcher.c',
    'src/config/toplevel.c',
    'src/wayland/foreign-toplevel.c',
    'src/wayland/idle-inhibit.c',
    'src/wayland/xdg-output.c',
    wayland_targets ]
dep = [gtk3, glib, gio_unix, gmod, glsh, wayl, json ]

if get_option('network').enabled() or get_option('network').auto()
  library('network', sources: 'modules/network.c', dependencies: glib,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
endif

if puls.found() and pulg.found()
  library('pulsectl', sources: 'modules/pulsectl.c',
      dependencies: [glib, puls, pulg],name_prefix: '',
      install: true, install_dir: get_option('libdir') / 'sfwbar' )
endif

if mpdc.found()
  library('mpd', sources: 'modules/mpd.c', dependencies: [glib, mpdc],
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
endif

if xkbr.found()
  library('xkbmap', sources: 'modules/xkbmap.c', dependencies: [glib, xkbr],
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
endif

if get_option('bsdctl').enabled() or get_option('bsdctl').auto()
  library('bsdctl', sources: 'modules/bsdctl.c', dependencies: glib,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
endif

executable ('sfwbar', sources: src,
    c_args: ['-DGLIB_DISABLE_DEPRECATION_WARNINGS',
    '-DGDK_DISABLE_DEPRECATED',
    '-DGTK_DISABLE_DEPRECATED'],
    dependencies: dep, install: true)

man_pages = [ 'doc/sfwbar.1',
  'doc/sfwbar-pulse.1',
  'doc/sfwbar-mpd.1',
  'doc/sfwbar-network.1',
  'doc/sfwbar-xkbmap.1'
]

foreach page : man_pages
  install_man(page)
endforeach
install_subdir('config',
    install_dir: get_option('datadir') / 'sfwbar', strip_directory : true )
install_subdir('icons/weather',
    install_dir: get_option('datadir') / 'sfwbar/icons' )
install_subdir('icons/misc',
    install_dir: get_option('datadir') / 'sfwbar/icons' )
install_data('icons/sfwbar.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps' )
