rustc = find_program('rustc')

ftree_src = files('ftree.rs')

custom_target(
  'ftree',
  input: ftree_src,
  output: 'ftree',
  command: [rustc, '-O', '-o', '@OUTPUT@', '@INPUT@'],
  install: true,
  install_dir: get_option('bindir'),
)
