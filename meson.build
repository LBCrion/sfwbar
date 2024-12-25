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
conf_data.set('conf_dir',
    get_option('prefix') / get_option('datadir') / 'sfwbar')
conf_data.set('locale_dir',
    get_option('prefix') / get_option('datadir') / 'locale')
configure_file(input: 'meson.h.meson', output: 'meson.h',
    configuration: conf_data )

wayland_scanner = find_program('wayland-scanner')
wayland_protodir = wayp.get_variable(pkgconfig: 'pkgdatadir')
wayland_protos = ['wlr-foreign-toplevel-management-unstable-v1',
                  'xdg-output-unstable-v1',
                  'wlr-layer-shell-unstable-v1',
                  'cosmic-workspace-unstable-v1',
                  'ext-workspace-v1',
]
wayland_targets=[]

foreach proto : wayland_protos
  wayland_targets += custom_target(
      input: 'protocols' / proto + '.xml', output:  proto + '.h',
      command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ])
  wayland_targets += custom_target(
      input: 'protocols' / proto + '.xml', output:  proto + '.c',
      command: [ wayland_scanner, 'public-code', '@INPUT@', '@OUTPUT@' ])
endforeach

headers = [ 'src' ]

libsrc = [
    'src/action.c',
    'src/actionlib.c',
    'src/appinfo.c',
    'src/client.c',
    'src/exprlib.c',
    'src/module.c',
    'src/mpd.c',
    'src/scanner.c',
    'src/signal.c',
    'src/sni.c',
    'src/sniitem.c',
    'src/snimenu.c',
    'src/trigger.c',
    'src/wayland.c',
    'src/window.c',
    'src/wintree.c',
    'src/workspace.c',
    'src/vm/expr.c',
    'src/vm/func.c',
    'src/vm/parser.c',
    'src/vm/vm.c',
    'src/gui/bar.c',
    'src/gui/basewidget.c',
    'src/gui/button.c',
    'src/gui/cchart.c',
    'src/gui/chart.c',
    'src/gui/css.c',
    'src/gui/flowgrid.c',
    'src/gui/flowitem.c',
    'src/gui/grid.c',
    'src/gui/image.c',
    'src/gui/label.c',
    'src/gui/menu.c',
    'src/gui/monitor.c',
    'src/gui/pager.c',
    'src/gui/pageritem.c',
    'src/gui/popup.c',
    'src/gui/scale.c',
    'src/gui/scaleimage.c',
    'src/gui/switcher.c',
    'src/gui/switcheritem.c',
    'src/gui/taskbar.c',
    'src/gui/taskbaritem.c',
    'src/gui/taskbarpager.c',
    'src/gui/taskbarpopup.c',
    'src/gui/taskbarshell.c',
    'src/gui/tray.c',
    'src/gui/trayitem.c',
    'src/config/base.c',
    'src/config/init.c',
    'src/config/layout.c',
    'src/config/placer.c',
    'src/config/scanner.c',
    'src/config/switcher.c',
    'src/config/toplevel.c',
    'src/ipc/cosmic-workspaces.c',
    'src/ipc/ext-workspace.c',
    'src/ipc/foreign-toplevel.c',
    'src/ipc/hyprland.c',
    'src/ipc/sway.c',
    'src/ipc/wayfire.c',
    'src/util/file.c',
    'src/util/json.c',
    'src/util/string.c',
    wayland_targets ]
deps = [gtk3, glib, gio_unix, gmod, glsh, wayl, json, lbrt ]
doc_list = [ 'sfwbar' ]

cargs = ['-DGLIB_DISABLE_DEPRECATION_WARNINGS',
    '-DGDK_DISABLE_DEPRECATED',
    '-DGTK_DISABLE_DEPRECATED']

sfwbar = library ('sfwbar', sources: libsrc,
    c_args: cargs, install_dir: get_option('libdir') / 'sfwbar',
    include_directories: headers,
    dependencies: deps, install: true)

deps = [deps, declare_dependency(link_with: sfwbar)]

if get_option('network').enabled() or get_option('network').auto()
  library('network', sources: 'modules/network.c', dependencies: deps,
      include_directories: headers,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-network'
endif

if get_option('bluez').enabled() or get_option('bluez').auto()
  library('bluez', sources: 'modules/bluez.c',
      include_directories: headers,
      dependencies: deps, name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-bluez'
endif

if get_option('iwd').enabled() or get_option('iwd').auto()
  library('wifi-iwd', sources: 'modules/wifi-iwd.c',
      include_directories: headers,
      dependencies: deps, name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-wifi'
endif

if get_option('nm').enabled() or get_option('nm').auto()
  library('wifi-nm', sources: 'modules/wifi-nm.c',
      include_directories: headers,
      dependencies: deps, name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  if 'sfwbar-wifi' not in doc_list
    doc_list += 'sfwbar-wifi'
  endif
endif

if get_option('ncenter').enabled() or get_option('ncenter').auto()
  library('ncenter', sources: 'modules/ncenter.c',
      include_directories: headers,
      dependencies: deps, name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-ncenter'
endif

if puls.found() and pulg.found()
  library('pulsectl', sources: 'modules/pulsectl.c',
      include_directories: headers,
      dependencies: [deps, puls, pulg],name_prefix: '',
      install: true, install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-volume'
endif

if mpdc.found()
  library('mpd', sources: 'modules/mpd.c', dependencies: [deps, mpdc],
      include_directories: headers,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-mpd'
endif

if alsa.found()
  library('alsactl', sources: 'modules/alsactl.c', dependencies: [deps, alsa],
      include_directories: headers,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  if 'sfwbar-volume' not in doc_list
    doc_list += 'sfwbar-volume'
  endif
endif

if xkbr.found()
  library('xkbmap', sources: 'modules/xkbmap.c', dependencies: [deps, xkbr],
      include_directories: headers,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-xkbmap'
endif

if get_option('appmenu').enabled()
  library('appmenu', sources: 'modules/appmenu.c', dependencies: deps,
      include_directories: headers,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-appmenu'
endif

if get_option('bsdctl').enabled()
  library('bsdctl', sources: 'modules/bsdctl.c', dependencies: deps,
      include_directories: headers,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-bsdctl'
endif

if get_option('idle').enabled() or get_option('idle').auto()
  idle_targets = [
    custom_target('ext-idle-notify-v1.h',
      output:'ext-idle-notify-v1.h',
      input: wayland_protodir /
        'staging/ext-idle-notify/ext-idle-notify-v1.xml',
      command: [ wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@' ] ),
    custom_target('ext-idle-notify-v1.c',
      output:'ext-idle-notify-v1.c',
      input: wayland_protodir /
        'staging/ext-idle-notify/ext-idle-notify-v1.xml',
      command: [ wayland_scanner, 'public-code', '@INPUT@', '@OUTPUT@' ] ) ]
  library('idle', sources: ['modules/idle.c',idle_targets], dependencies: deps,
      include_directories: headers,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-idle'
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
      include_directories: headers,
      name_prefix: '', install: true,
      install_dir: get_option('libdir') / 'sfwbar' )
  doc_list += 'sfwbar-idleinhibit'
endif

executable ('sfwbar', sources: 'src/sfwbar.c',
    include_directories: headers,
    c_args: ['-DGLIB_DISABLE_DEPRECATION_WARNINGS',
    '-DGDK_DISABLE_DEPRECATED',
    '-DGTK_DISABLE_DEPRECATED'],
    install_rpath: get_option('prefix') / get_option('libdir') / 'sfwbar',
    dependencies: [deps], install: true)

build_docs = get_option('build-docs')
rst2man = find_program('rst2man', 'rst2man.py',
    required: build_docs )
foreach doc: doc_list
  if rst2man.found() and not get_option('build-docs').disabled()
    custom_target(input: 'doc' / doc + '.rst' , output: '@BASENAME@.1',
        command: [rst2man, '@INPUT@', '@OUTPUT@' ],
        install: true,
        install_dir: get_option('mandir') / 'man1' )
  else
      install_man('doc' / doc + '.1' )
  endif
endforeach

install_subdir('config',
    install_dir: get_option('datadir') / 'sfwbar', strip_directory : true )
install_subdir('icons/weather',
    install_dir: get_option('datadir') / 'sfwbar/icons' )
install_subdir('icons/misc',
    install_dir: get_option('datadir') / 'sfwbar/icons' )
install_data('icons/sfwbar.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps' )
install_subdir('data/desktop-directories',
    install_dir: get_option('datadir') / 'sfwbar' )
