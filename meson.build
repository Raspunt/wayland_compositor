project('flottywm', 'c',
  version: '0.1.0',
  default_options: [
    'warning_level=2',
    'c_std=c11',
  ]
)

wlroots = dependency('wlroots-0.18')
wayland_server = dependency('wayland-server')
wayland_scanner = find_program('wayland-scanner')
wayland_protocols = dependency('wayland-protocols')

# Добавляем xkbcommon (нужен для keyboard.c)
xkbcommon = dependency('xkbcommon')

protocols_dir = wayland_protocols.get_variable('pkgdatadir')

xdg_shell_h = custom_target(
    'xdg-shell-protocol',
    output: 'xdg-shell-protocol.h',
    input: protocols_dir / 'stable/xdg-shell/xdg-shell.xml',
    command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

layer_shell_h = custom_target(
    'wlr-layer-shell-protocol',
    output: 'wlr-layer-shell-unstable-v1-protocol.h',
    input: 'protocols/wlr-layer-shell-unstable-v1.xml',
    command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@']
)

src_files = files(
  'src/main.c',
  'src/output.c',
  'src/cursor.c',
  'src/keyboard.c',
  'src/input.c',
  'src/renderer.c',
  'src/layer_shell.c',
  'src/config.c'
)

configure_file(
  input: 'config.default',
  output: 'config.default',
  copy: true
)

executable('flottywm',
  src_files, xdg_shell_h, layer_shell_h,
  include_directories: include_directories('src', '.', 'build'),
  dependencies: [wlroots, wayland_server, xkbcommon],
  c_args: ['-DWLR_USE_UNSTABLE'],
  install: true
)