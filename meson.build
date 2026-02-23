# The Storcke Human Time System- By Antonio Storcke.
project('com.storcke64.hts_time', 'c', version: '1.0.0')

gtk_dep = dependency('gtk4')
webkit_dep = dependency('webkitgtk-6.0')
glib_dep = dependency('glib-2.0')
qrencode_dep = dependency('libqrencode')
microhttpd_dep = dependency('libmicrohttpd')

# --- Data and Asset Installation ---

# This installs the 'data' folder (containing themes/HTML) into /app/bin/data
install_subdir('data', install_dir: 'bin')

# This installs the doctor utility
install_subdir('hts-doctor', install_dir: 'bin')

# --- System Integration (The Fix) ---

# Explicitly installing the DESKTOP file to the standard Flatpak export path
# We ensure the source is the .desktop file located in the root project directory
install_data(
    'com.storcke64.hts_time.desktop', 
    install_dir: get_option('datadir') / 'applications'
)

# Explicitly installing the ICON
install_data(
    'data/com.storcke64.hts_time.svg', 
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps'
)

subdir('src')
