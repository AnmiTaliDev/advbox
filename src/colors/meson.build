rustc = find_program('rustc')

colors_src = files('colors.rs')

custom_target(
  'colors',
  input: colors_src,
  output: 'colors',
  command: [rustc, '-O', '-o', '@OUTPUT@', '@INPUT@'],
  install: true,
  install_dir: get_option('bindir'),
)
