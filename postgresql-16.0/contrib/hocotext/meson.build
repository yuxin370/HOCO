# Copyright (c) 2023, Group

hocotext_sources = files(
    'hocotext.c',
)

if host_system == 'windows'
  hocotext_sources += rc_lib_gen.process(win32ver_rc, extra_args: [
    '--NAME', 'hocotext',
    '--FILEDESC', 'hocotext - Homomorphic Compression Schemes data type',])
endif

hocotext = shared_module('hocotext',
  hocotext_sources,
  include_directories: include_directories('.'),
  kwargs: contrib_mod_args,
)
contrib_targets += hocotext

install_data(
    'hocotext.control',
    'hocotext--1.0.sql',
    kwargs: contrib_data_args,
)

tests += {
    'name': 'hocotext',
    'sd': meson.current_source_dir(),
    'bd': meson.current_build_dir(),
    'regress': {
        'sql': [
            'hocotext',
            'hocotext_utf8',

        ],
    },
}
