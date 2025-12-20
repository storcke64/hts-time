gnome = import('gnome')

# 1. Dependencies
gtk4_dep = dependency('gtk4')
gio_dep = dependency('gio-2.0')

# 2. Resources (Logo for internal use)
hts_resources = gnome.compile_resources(
    'hts-resources',
    'resources.xml',
    source_dir: [
        meson.project_source_root() / 'data',
        meson.current_source_dir()
    ],
    c_name: 'hts'
)

# 3. Executable
executable(
    'hts-time',
    'main.c',
    'hts-logic.c',
    hts_resources,
    dependencies: [gtk4_dep, gio_dep],
    install: true
)

# 4. Install Desktop File (Updated to look in Project Root)
install_data(
    meson.project_source_root() / 'com.storcke64.hts_time.desktop',
    install_dir: get_option('datadir') / 'applications'
)

# 5. Install Icon (Looks in data/ for the logo)
install_data(
    meson.project_source_root() / 'data' / 'logo.png',
    install_dir: get_option('datadir') / 'icons/hicolor/256x256/apps',
    rename: 'com.storcke64.hts_time.png'
)