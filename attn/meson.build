# async gpio monitor needs boost library
boost_args = [
    '-DBOOST_ASIO_DISABLE_THREADS',
    '-DBOOST_ERROR_CODE_HEADER_ONLY',
    '-DBOOST_SYSTEM_NO_DEPRECATED',
]

# dependency to link gpiod support
libgpiod = dependency('libgpiod', version: '>=1.4.1')

# get openpower-hw-diags build configuration
attn_conf = configuration_data()

# conditionally define ENABLE_PHAL_TRUE
if (get_option('phal').allowed())
    attn_conf.set('ENABLE_PHAL_TRUE', '')
else
    attn_conf.set('ENABLE_PHAL_TRUE', '#')
endif

# attention handler default config override
attn_conf.set('ATTN_CONFIG', get_option('attn_config'))

# install attention handler unit file
configure_file(
    configuration: attn_conf,
    input: 'attn_handler.service.in',
    output: 'attn_handler.service',
    install: true,
    install_dir: dependency('systemd').get_variable('systemd_system_unit_dir'),
)

# install attention handler config file
configure_file(
    configuration: attn_conf,
    input: 'attn_config.in',
    output: 'attn_config',
    install: true,
    install_dir: join_paths(get_option('sysconfdir'), 'default'),
)

# Source files.
attn_src = files(
    'attention.cpp',
    'attn_common.cpp',
    'attn_config.cpp',
    'attn_dbus.cpp',
    'attn_dump.cpp',
    'attn_handler.cpp',
    'attn_logging.cpp',
    'attn_main.cpp',
    'bp_handler.cpp',
    'fsi_attn_monitor.cpp',
    'gpio_attn_monitor.cpp',
    'ti_handler.cpp',
    'vital_handler.cpp',
)

# for custom/raw PEL creation
pel_src = files(
    'pel/extended_user_header.cpp',
    'pel/pel_minimal.cpp',
    'pel/primary_src.cpp',
    'pel/private_header.cpp',
    'pel/user_header.cpp',
)

# Library dependencies.
attn_deps = [libgpiod, libpdbg_dep, phosphor_logging_dep, sdbusplus_dep]

# conditional library dependencies
if get_option('phal').allowed()
    attn_deps += libphal_dep
endif

# Create static library.
attn_lib = static_library(
    'attn_lib',
    attn_src,
    pel_src,
    include_directories: incdir,
    dependencies: attn_deps,
    cpp_args: [boost_args],
    install: false,
)
