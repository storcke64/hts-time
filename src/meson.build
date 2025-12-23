hts_sources = [
  'main.c',
  'hts-logic.c',
]

executable('hts-time',
  hts_sources,
  hts_resources,
  dependencies: [
    gtk4_dep,
    glib_dep,
    gio_dep,
    qrencode_dep,
  ],
  install: true
)