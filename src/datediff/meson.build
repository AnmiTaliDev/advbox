rustc = find_program('rustc')

datediff_src = files('datediff.rs')

custom_target(
  'datediff',
  input: datediff_src,
  output: 'datediff',
  command: [rustc, '-O', '-o', '@OUTPUT@', '@INPUT@'],
  install: true,
  install_dir: get_option('bindir'),
)
