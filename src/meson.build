# --- src/meson.build ---

# Define the sources for our single universal binary
# We only need main.c and hts-logic.c now
hts_sources = [
  'main.c',
  'hts-logic.c',
]

# Create the master executable 'hts-time'
# This matches the "command" field in your .json manifest
executable('hts-time',
  hts_sources,
  hts_resources, # Compiled GResources from root meson.build
  dependencies: [
    gtk4_dep,
    glib_dep,
    gio_dep,
    qrencode_dep
  ],
  install: true
)