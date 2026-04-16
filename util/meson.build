# Source files.
util_src = files(
    'data_file.cpp',
    'dbus.cpp',
    'ffdc.cpp',
    'ffdc_file.cpp',
    'pdbg-no-sim.cpp',
    'pdbg.cpp',
    'pldm.cpp',
    'temporary_file.cpp',
)

# Library dependencies.
util_deps = [
    libhei_dep,
    libpldm_dep,
    nlohmann_json_dep,
    phosphor_logging_dep,
    valijson_dep,
    phal_dep,
]

if phal_backend == 'legacy'
    util_deps += dependency('libdt-api')
endif

# Create static library.
util_lib = static_library(
    'util_lib',
    util_src,
    include_directories: incdir,
    dependencies: util_deps,
    cpp_args: [package_args],
    install: false,
)

# Install the util data files
subdir('data')
