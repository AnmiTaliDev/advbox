rustc = find_program('rustc')

killport_src = files('killport.rs')

custom_target(
  'killport',
  input: killport_src,
  output: 'killport',
  command: [rustc, '-O', '-o', '@OUTPUT@', '@INPUT@'],
  install: true,
  install_dir: get_option('bindir'),
)
