# See README.md for details.
project(
    'openpower-hw-diags',
    'cpp',
    version: '0.1',
    meson_version: '>=1.1.1',
    default_options: ['warning_level=3', 'werror=true', 'cpp_std=c++23'],
)

# Package directory root, which will contain required data files.
package_dir = join_paths(
    get_option('prefix'),
    get_option('datadir'),
    meson.project_name(),
)

# Compiler option so that source knows the package directory.
package_args = ['-DPACKAGE_DIR="' + package_dir + '/"']

#-------------------------------------------------------------------------------
# Versioning
#-------------------------------------------------------------------------------
buildinfo = vcs_tag(
    command: ['git', 'describe', '--always', '--long'],
    input: 'buildinfo.hpp.in',
    output: 'buildinfo.hpp',
    replace_string: '@BUILDINFO@',
    fallback: '0',
)

#-------------------------------------------------------------------------------
# Compiler
#-------------------------------------------------------------------------------

cmplr = meson.get_compiler('cpp')

#-------------------------------------------------------------------------------
# Config file
#-------------------------------------------------------------------------------

conf = configuration_data()

# OpenPOWER dump object path override
conf.set_quoted('OP_DUMP_OBJ_PATH', get_option('op_dump_obj_path'))

conf.set('CONFIG_PHAL_API', get_option('phal').allowed())

system_config = get_option('system_config')

if system_config == 'p10'
    conf.set('CONFIG_P10', true)
else
    conf.set('CONFIG_P10', false)
endif

if system_config == 'p11'
    conf.set('CONFIG_P11', true)
else
    conf.set('CONFIG_P11', false)
endif

if system_config == 'p12'
    conf.set('CONFIG_P12', true)
else
    conf.set('CONFIG_P12', false)
endif

if (get_option('transport-implementation')) == 'mctp-demux'
    add_project_arguments('-DPLDM_TRANSPORT_WITH_MCTP_DEMUX', language: 'cpp')
else
    add_project_arguments('-DPLDM_TRANSPORT_WITH_AF_MCTP', language: 'cpp')
endif

if cmplr.has_header('poll.h')
    add_project_arguments('-DPLDM_HAS_POLL=1', language: 'cpp')
endif

configure_file(input: 'config.h.in', output: 'config.h', configuration: conf)

#-------------------------------------------------------------------------------
# Include directories
#-------------------------------------------------------------------------------

# Only using the base directory. All header includes should provide the full
# path from the base directory.
incdir = include_directories('.')

#-------------------------------------------------------------------------------
# External library dependencies
#-------------------------------------------------------------------------------

# Look if the libhei library has already been built and installed. If not,
# default to the subproject.
libhei_dep = dependency('hei', fallback: ['libhei', 'libhei_dep'])

phosphor_logging_dep = dependency(
    'phosphor-logging',
    fallback: ['phosphor-logging', 'phosphor_logging_dep'],
)

sdbusplus_dep = dependency('sdbusplus', version: '>=1.0')
dbus_interfaces_dep = dependency('phosphor-dbus-interfaces')

libpdbg_dep = dependency('pdbg')

if get_option('phal').allowed()
    libphal_dep = cmplr.find_library('phal')
endif

libpldm_dep = dependency('libpldm')

pthread = declare_dependency(link_args: '-pthread')
lrt = declare_dependency(link_args: '-lrt')

# JSON parser
nlohmann_json_dep = dependency('nlohmann_json', include_type: 'system')

# JSON validator
if cmplr.has_header('valijson/validator.hpp')
    valijson_dep = declare_dependency()
else
    valijson_dep = dependency('valijson', include_type: 'system')
endif

#-------------------------------------------------------------------------------
# Build the local static libraries
#-------------------------------------------------------------------------------

subdir('analyzer')
subdir('attn')
subdir('util')

hwdiags_libs = [analyzer_lib, attn_lib, util_lib]

#-------------------------------------------------------------------------------
# Build the executable
#-------------------------------------------------------------------------------

no_listener_mode = get_option('nlmode')

if no_listener_mode.allowed()
    executable(
        'openpower-hw-diags',
        sources: ['main_nl.cpp', 'cli.cpp', buildinfo, plugins_src],
        dependencies: [libhei_dep, nlohmann_json_dep, phosphor_logging_dep],
        link_with: hwdiags_libs,
        install: true,
    )
else
    executable(
        'openpower-hw-diags',
        sources: ['main.cpp', 'cli.cpp', 'listener.cpp', buildinfo, plugins_src],
        dependencies: [
            lrt,
            pthread,
            libhei_dep,
            nlohmann_json_dep,
            phosphor_logging_dep,
        ],
        link_with: hwdiags_libs,
        install: true,
    )
endif

#-------------------------------------------------------------------------------
# Test, if configured
#-------------------------------------------------------------------------------

build_tests = get_option('tests')

if build_tests.allowed()

    # IMPORTANT NOTE:
    # We cannot link the test executables to `util_lib` because:
    #   - It is built without `-DTEST_TRACE` and any of the util functions that
    #     use `trace.hpp` will throw a linker error because we don't have access
    #     to phosphor-logging in test ... yet. This issue will go away once we
    #     have converted all of our trace to use the `lg2` interfaces.
    #   - Some functions related to pdbg and dbus simply cannot be built in the
    #     test environment. Instead, there are alternate implementation of those
    #     functions to simulate them for testing (see `test/*-sim-only.cpp`).
    # Instead we will build a `test_util_lib` that will contain the `util` files
    # that we need in test along with simulated versions of some util functions.

    # IMPORTANT NOTE:
    # When running GCOV reports, the Jenkins CI script explicitly ignores any
    # libraries and executables built in the `test/` directory. Therefore, this
    # `test_util_lib` library must be built here instead in order to get any GCOV
    # credit for the code.

    test_args = ['-DTEST_TRACE', package_args]

    test_util_srcs = [
        files(
            'test/dbus-sim-only.cpp',
            'test/pdbg-sim-only.cpp',
            'util/data_file.cpp',
            'util/ffdc_file.cpp',
            'util/pdbg.cpp',
            'util/temporary_file.cpp',
        ),
    ]

    test_util_deps = [
        libhei_dep,
        libpdbg_dep,
        nlohmann_json_dep,
        phosphor_logging_dep,
        valijson_dep,
    ]

    test_util_lib = static_library(
        'test_util_lib',
        sources: test_util_srcs,
        include_directories: incdir,
        dependencies: test_util_deps,
        cpp_args: test_args,
        install: true,
    )

    test_libs = [analyzer_lib, attn_lib, test_util_lib]

    subdir('test')
endif
