# --- meson.build ---

project('hts-time', 'c',
  version: '1.2.0',
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

# Compile GResources
# This pulls in your logo and any XML UI definitions
hts_resources = gnome.compile_resources(
  'hts-resources',
  'src/resources.xml',
  source_dir: 'data',
)

# Descend into the src directory to build the universal binary
subdir('src')

# --- INSTALLATION SECTION ---

# 1. Install Desktop File
# This makes the app appear in the Linux application menu
install_data('com.storcke64.hts_time.desktop',
  install_dir: get_option('datadir') / 'applications'
)

# 2. Install Application Icon
# We rename logo.png to the App ID so the system pairs it with the desktop file
install_data('data/logo.png',
  rename: 'com.storcke64.hts_time.png',
  install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps'
)

# 3. Install Metainfo File
# Source file: com.storcke64.hts-time.metainfo.xml (with dash)
# Destination: com.storcke64.hts_time.metainfo.xml (with underscore)
install_data('data/com.storcke64.hts-time.metainfo.xml',
  rename: 'com.storcke64.hts_time.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo'
)

# 4. Install Man Page
# This allows the user to type 'man hts-time' in the terminal
install_man('hts-time.1')