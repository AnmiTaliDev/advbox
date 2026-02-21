rustc = find_program('rustc')

custom_target(
  'extract',
  input: files('extract.rs'),
  output: 'extract',
  command: [rustc, '-O', '-o', '@OUTPUT@', '@INPUT@'],
  install: true,
  install_dir: get_option('bindir'),
)
