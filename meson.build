project('hts-time', 'c',
  version: '0.1.0',
  meson_version: '>= 1.0.0',
  default_options: [ 'warning_level=2', 'werror=false', 'c_std=gnu11', ],
)

# --- Compiler and Warning Arguments ---
cc = meson.get_compiler('c')

project_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  '-Werror=address',
  '-Werror=array-bounds',
  '-Werror=empty-body',
  '-Werror=implicit',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=int-to-pointer-cast',
  '-Werror=main',
  '-Werror=misleading-indentation',
  '-Werror=missing-braces',
  '-Werror=missing-include-dirs',
  '-Werror=nonnull',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=pointer-arith',
  '-Werror=pointer-to-int-cast',
  '-Werror=redundant-decls',
  '-Werror=return-type',
  '-Werror=sequence-point',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',

  # WARNING FIX: Explicitly disable the Wformat-nonliteral warning
  '-Wno-format-nonliteral',

  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-dangling-pointer',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-fno-strict-aliasing',
  ['-Werror=format-security', '-Werror=format=2'],
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif
foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    project_c_args += arg
  endif
endforeach
add_project_arguments(project_c_args, language: 'c')

# --- DEFINITION OF EXECUTABLE ---
# Source file location is relative to this meson.build (the root)
src_files = ['src/main.c']

executable(
    'hts-time',
    src_files,
    link_args: ['-lm'],
    install: true
)

# --- INSTALL APPLICATION ICON (SVG) ---
# Installing the icon with the hyphenated ID to /app/share/icons/...
install_data(
    files('data/icons/hicolor/scalable/apps/com.storcke64.hts-time.svg'),
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps'
)

# --- INSTALL DESKTOP FILE ---
# This file registers the app name and icon with the desktop environment.
install_data(
    files('com.storcke64.hts-time.desktop'),
    install_dir: get_option('datadir') / 'applications'
)

# --- INSTALL MAN PAGE ---
# Installing the man page from hts-time.1 in the project root.
install_man('hts-time.1')