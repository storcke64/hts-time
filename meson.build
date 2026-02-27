# The Storcke Human Time System - By Antonio Storcke.
project('com.storcke64.hts_time', 'c', version: '1.0.0')

gtk_dep = dependency('gtk4')
webkit_dep = dependency('webkitgtk-6.0')
glib_dep = dependency('glib-2.0')
qrencode_dep = dependency('libqrencode')
microhttpd_dep = dependency('libmicrohttpd')

install_subdir('data', install_dir: get_option('datadir') / 'hts-time')

install_data(
    'com.storcke64.hts_time.desktop',
    install_dir: get_option('datadir') / 'applications'
)

install_data(
    'data/com.storcke64.hts_time.svg',
    install_dir: get_option('datadir') / 'icons/hicolor/scalable/apps'
)

subdir('src')
