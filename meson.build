project('simple', 'c')

wlroots = dependency('wlroots-0.18')
wayland_server = dependency('wayland-server')

executable('myexe', 'src/main.c', 
  dependencies: [wlroots, wayland_server],  # или dependencies: wlroots, wayland_server,
  c_args: '-DWLR_USE_UNSTABLE'
)