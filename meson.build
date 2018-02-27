project(
  'fum', 'c',
  meson_version : '>=0.36.0',
)

dep_libsolv = dependency('libsolv')
dep_libsolvext = dependency('libsolvext')

executable(
  'installcheck',
  sources : 'installcheck.c',
  dependencies : [
    dep_libsolv,
    dep_libsolvext,
  ],
)
