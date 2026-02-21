rustc = find_program('rustc')

estimate_src = files('estimate.rs')

custom_target(
  'estimate',
  input: estimate_src,
  output: 'estimate',
  command: [rustc, '-O', '-o', '@OUTPUT@', '@INPUT@'],
  install: true,
  install_dir: get_option('bindir'),
)
