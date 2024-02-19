project('sfwbar','c')
cc = meson.get_compiler('c')
lbrt = cc.find_library('rt', required: false)
gtk3 = dependency('gtk+-3.0')
glib = dependency('glib-2.0')
ggio = dependency('gio-2.0')
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
alsa = dependency('alsa', required: get_option('alsa'))

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
  'protocols/wlr-layer-shell-unstable-v1'
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

libsrc = [
    'src/action.c',
    'src/actionlib.c',
    'src/bar.c',
    'src/basewidget.c',
    'src/button.c',
    'src/cchart.c',
    'src/chart.c',
    'src/client.c',
    'src/css.c',
    'src/expr.c',
    'src/exprlib.c',
    'src/flowgrid.c',
    'src/flowitem.c',
    'src/grid.c',
    'src/hypr_ipc.c',
    'src/image.c',
    'src/jpath.c',
    'src/label.c',
    'src/menu.c',
    'src/misc.c',
    'src/module.c',
    'src/mpd.c',
    'src/pager.c',
    'src/pageritem.c',
    'src/popup.c',
    'src/scale.c',
    'src/scaleimage.c',
    'src/scanner.c',
    'src/signal.c',
    'src/sni.c',
    'src/sniitem.c',
    'src/snimenu.c',
    'src/sway_ipc.c',
    'src/switcher.c',
    'src/switcheritem.c',
    'src/taskbar.c',
    'src/taskbaritem.c',
    'src/taskbarpager.c',
    'src/taskbarpopup.c',
    'src/tray.c',
    'src/trayitem.c',
    'src/wayland.c',
    'src/window.c',
    'src/wintree.c',
    'src/workspace.c',
    'src/config/base.c',
    'src/config/init.c',
    'src/config/layout.c',
    'src/config/placer.c',
    'src/config/scanner.c',
    'src/config/switcher.c',
    'src/config/toplevel.c',
    'src/wayland/foreign-toplevel.c',
    'src/wayland/probe.c',
    'src/wayland/xdg-output.c',
    wayland_targets ]
deps = [gtk3, glib, gio_unix, gmod, glsh, wayl, json, lbrt ]
man_pages = [ 'doc/sfwbar.1' ]

cargs = ['-DGLIB_DISABLE_DEPRECATION_WARNINGS',
    '-DGDK_DISABLE_DEPRECATED',
    '-DGTK_DISABLE_DEPRECATED']

sfwbar = library ('sfwbar', sources: libsrc,
    c_args: cargs, install_dir: get_option('libdir') / 'sfwbar',
    dependencies: deps, install: true)

deps = [deps, declare_dependency(link_with: sfwbar)]

if get_option('network').enabled() or get_option('network').auto()
  library('network', sources: 'modules/network.c', dependencies: deps,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  man_pages += 'doc/sfwbar-network.1'
endif

if get_option('bluez').enabled() or get_option('bluez').auto()
  library('bluez', sources: 'modules/bluez.c',
      dependencies: deps, name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  man_pages += 'doc/sfwbar-bluez.1'
endif

if get_option('iwd').enabled() or get_option('iwd').auto()
  library('wifi-iwd', sources: 'modules/wifi-iwd.c',
      dependencies: deps, name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  man_pages += 'doc/sfwbar-wifi.1'
endif

if get_option('nm').enabled() or get_option('nm').auto()
  library('wifi-nm', sources: 'modules/wifi-nm.c',
      dependencies: deps, name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  man_pages += 'doc/sfwbar-wifi.1'
endif

if puls.found() and pulg.found()
  library('pulsectl', sources: 'modules/pulsectl.c',
      dependencies: [deps, puls, pulg],name_prefix: '',
      install: true, install_dir: get_option('libdir') / 'sfwbar' )
  man_pages += 'doc/sfwbar-pulse.1'
endif

if mpdc.found()
  library('mpd', sources: 'modules/mpd.c', dependencies: [deps, mpdc],
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  man_pages += 'doc/sfwbar-mpd.1'
endif

if alsa.found()
  library('alsactl', sources: 'modules/alsactl.c', dependencies: [deps, alsa],
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  man_pages += 'doc/sfwbar-alsa.1'
endif

if xkbr.found()
  library('xkbmap', sources: 'modules/xkbmap.c', dependencies: [deps, xkbr],
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  man_pages += 'doc/sfwbar-xkbmap.1'
endif

if get_option('bsdctl').enabled()
  library('bsdctl', sources: 'modules/bsdctl.c', dependencies: deps,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  man_pages += 'doc/sfwbar-bsdctl.1'
endif

if get_option('idleinhibit').enabled() or get_option('idleinhibit').auto()
  inhibit_targets = [
    custom_target('idle-inhibit-unstable-v1.h',
      output:'idle-inhibit-unstable-v1.h',
      input: wayland_protodir /
        'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml',
      command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ] ),
    custom_target('idle-inhibit-unstable-v1.c',
      output:'idle-inhibit-unstable-v1.c',
      input: wayland_protodir /
        'unstable/idle-inhibit/idle-inhibit-unstable-v1.xml',
      command: [ wayland_scanner, 'public-code', '@INPUT@', '@OUTPUT@' ] ) ]

  library('idleinhibit', sources: ['modules/idleinhibit.c',inhibit_targets], dependencies: deps,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  man_pages += 'doc/sfwbar-idleinhibit.1'
endif

executable ('sfwbar', sources: 'src/sfwbar.c',
    c_args: ['-DGLIB_DISABLE_DEPRECATION_WARNINGS',
    '-DGDK_DISABLE_DEPRECATED',
    '-DGTK_DISABLE_DEPRECATED'],
    install_rpath: get_option('prefix') / get_option('libdir') / 'sfwbar',
    dependencies: [deps], install: true)

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
