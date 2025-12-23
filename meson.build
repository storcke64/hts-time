project('hts-time', 'c',
  version: '1.1.9',
  default_options: [
    'warning_level=2',
    'c_std=gnu11',
  ]
)

gnome = import('gnome')

# Global Dependencies
gtk4_dep = dependency('gtk4')
glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')
qrencode_dep = dependency('libqrencode')

# Compile resources using the XML in /src
# We point source_dir to the project root so it can find data/logo.png if needed
# or 'data' if the XML just says 'logo.png'
hts_resources = gnome.compile_resources(
  'hts-resources',
  'src/resources.xml',
  source_dir: 'data',
)

subdir('src')

install_data('com.storcke64.hts_time.desktop',
  install_dir: get_option('datadir') / 'applications'
)