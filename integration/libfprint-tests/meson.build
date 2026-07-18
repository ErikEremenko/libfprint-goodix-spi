# Deterministic out-of-tree tests copied into tests/goodix-spi/ by install.sh.
goodix_out_of_tree_tests = {
    'goodix-wire' : [
        'goodix-common/test-goodix-wire.c',
        '../../libfprint/lib/goodix/common/goodix-wire.c',
    ],
    'goodix-sensor-profile' : [
        'goodix-common/test-goodix-sensor-profile.c',
        '../../libfprint/lib/goodix/common/goodix-sensor-profile.c',
    ],
    'goodix-command' : [
        'goodix-common/test-goodix-command.c',
        '../../libfprint/lib/goodix/common/goodix-command.c',
        '../../libfprint/lib/goodix/common/goodix-wire.c',
    ],
    'goodix-mcu-config' : [
        'goodix-common/test-goodix-mcu-config.c',
        '../../libfprint/lib/goodix/common/goodix-mcu-config.c',
    ],
    'goodix-image' : [
        'goodix-common/test-goodix-image.c',
        '../../libfprint/lib/goodix/common/goodix-image.c',
    ],
    'goodix-crc' : [
        'goodix-common/test-goodix-crc.c',
        '../../libfprint/lib/goodix/common/goodix-crc.c',
    ],
    'goodix-chicago-template' : [
        'goodix-chicago/test-goodix-chicago-template.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-template.c',
        '../../libfprint/lib/goodix/common/goodix-crc.c',
    ],
    'goodix-chicago-bir' : [
        'goodix-chicago/test-goodix-chicago-bir.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-bir.c',
    ],
    'goodix-chicago-calibration' : [
        'goodix-chicago/test-goodix-chicago-calibration.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-calibration.c',
    ],
    'goodix-chicago-preprocess' : [
        'goodix-chicago/test-goodix-chicago-preprocess.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-calibration.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-preprocess.c',
    ],
    'goodix-chicago-feature' : [
        'goodix-chicago/test-goodix-chicago-feature.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-feature.c',
    ],
    'goodix-chicago-enrollment' : [
        'goodix-chicago/test-goodix-chicago-enrollment.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-enrollment.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-feature.c',
    ],
    'goodix-chicago-match' : [
        'goodix-chicago/test-goodix-chicago-match.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-late-rejection-compat.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-late-rejection.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-match.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-runtime.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-enrollment.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-feature.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-preprocess.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-calibration.c',
        '../../libfprint/lib/goodix/chicago/goodix-chicago-template.c',
        '../../libfprint/lib/goodix/common/goodix-crc.c',
    ],
}

goodix_include_directories = include_directories(
    '../../libfprint/lib/goodix/common',
    '../../libfprint/lib/goodix/chicago',
)

foreach test_name, test_sources: goodix_out_of_tree_tests
    test_executable = executable('test-' + test_name,
        sources: test_sources,
        dependencies: [ libfprint_private_dep ],
        include_directories: goodix_include_directories,
        c_args: common_cflags,
    )
    test(test_name, test_executable,
        suite: ['unit-tests', 'goodix-spi'],
        env: envs,
    )
endforeach
