# Storcke Human Time System - HTS Universal Tool
# Inventor: Antonio Storcke
# File: src/meson.build

gst_dep = dependency('gstreamer-1.0')
gst_video_dep = dependency('gstreamer-video-1.0')
gst_app_dep = dependency('gstreamer-app-1.0')

sources = files('main.c', 'hts-logic.c')

c_args = [
  '-D_GNU_SOURCE',
  '-DDATADIR="' + get_option('prefix') / get_option('datadir') / 'hts-time' + '"',
  '-DPACKAGE_NAME="com.storcke64.hts_time"'
]

executable('hts-time', sources,
  dependencies: [
    gtk_dep,
    webkit_dep,
    glib_dep,
    qrencode_dep,
    microhttpd_dep,
    gst_dep,
    gst_video_dep,
    gst_app_dep
  ],
  install: true,
  c_args: c_args,
  export_dynamic: true
)
