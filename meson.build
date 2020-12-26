project('sfwbar','c')
gtk3 = dependency('gtk+-3.0')
glib = dependency('glib-2.0')
json = dependency('json-c')
glsh = dependency('gtk-layer-shell-0')

src = ['src/sfwbar.c',
    'src/ipc.c',
    'src/placement.c',
    'src/layout.c',
    'src/taskbar.c',
    'src/scanner.c',
    'src/expr.c',
    'src/misc.c',
    'src/clampgrid.c',
    'src/parser.tab.c',
    'src/lex.yy.c']
dep = [gtk3, glib, json, glsh ]

executable ('sfwbar', sources: src, dependencies: dep, install: true)

install_man('sfwbar.1')
install_data(['config/sfwbar.config','config/sfwbar.css'],
    rename: ['sfwbar.config','sfwbar.css'],
    install_dir: get_option('datadir') / 'sfwbar')


