# Get the gtest/gmock dependencies.
gtest_dep = dependency('gtest', main: true, disabler: true, required: false)
gmock_dep = dependency('gmock', disabler: true, required: false)
if not gtest_dep.found() or not gmock_dep.found()
    cmake = import('cmake')
    gtest_proj = cmake.subproject('googletest',
                                  required: false)
    if gtest_proj.found()
        gtest_dep = declare_dependency(
            dependencies: [
                dependency('threads'),
                gtest_proj.dependency('gtest'),
                gtest_proj.dependency('gtest_main'),
            ]
        )
        gmock_dep = gtest_proj.dependency('gmock')
    else
        assert(not get_option('tests').allowed(),
               'Googletest is required if tests are enabled')
    endif
endif

################################################################################

# Compile the test dts into a binary for pdbg.
pdbg_test_dtb = custom_target('build_pdbg_test_dtb',
    input   : files('pdbg-test.dts'),
    output  : 'pdbg-test.dtb',
    command : [ find_program('dtc'), '-I', 'dts', '-O', 'dtb',
                '-o', '@OUTPUT@', '@INPUT@' ])

pdbg_env = 'PDBG_DTB=' + pdbg_test_dtb.full_path()

################################################################################

# Add gtest/gmock dependency to the list of test dependencies.
test_deps = [
  test_util_deps,
  gtest_dep,
]

test_vars = [
  pdbg_env,
  'LG2_FORCE_STDERR=true',
]

# Additional SRCs that are not (or should not be) included in libraries.
# NOTE: Try to limit this, if possible, to prevent duplicate compilation.
test_additional_srcs = [
  files(
    '../analyzer/filter-root-cause.cpp',
    '../analyzer/plugins/ody-plugins.cpp',
    '../analyzer/plugins/p10-plugins.cpp',
    '../analyzer/plugins/p10-tod-plugins.cpp',
    '../cli.cpp',
  ),
  pdbg_test_dtb
]

################################################################################

testcases = [
    'test-bin-stream',
    'test-ffdc-file',
    'test-lpc-timeout',
    'test-pdbg-dts',
    'test-pll-unlock',
    'test-resolution',
    'test-root-cause-filter',
    'test-tod-step-check-fault',
    'test-cli',
    'test-chnl-timeout',
]

# allow more time for long running tests
longtests = {'test-pll-unlock': 2, 'test-chnl-timeout': 2}

foreach tc : testcases

  exe = executable(tc.underscorify(),
    sources             : [ files(tc + '.cpp'), test_additional_srcs ],
    include_directories : incdir,
    dependencies        : test_deps,
    cpp_args            : test_args,
    link_with           : test_libs,
  )

  test(tc, exe, env: test_vars, timeout: 30 * longtests.get(tc, 1))

endforeach

################################################################################

testcases = [
  'test-attention',
  'test-end2end',
  'test-util-data-file',
  'test-ti-handler',
]

# allow more time for long running tests
longtests = {
  'test-end2end': 2,
}

foreach tc : testcases

  exe = executable(tc.underscorify(),
    sources             : [ files(tc + '.cpp'), test_additional_srcs ],
    include_directories : incdir,
    dependencies        : test_deps,
    cpp_args            : test_args,
    link_with           : hwdiags_libs, # TODO: should use test_libs instead
  )

  test(tc, exe, env: test_vars, timeout: 30 * longtests.get(tc, 1))

endforeach

