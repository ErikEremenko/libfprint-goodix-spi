# Internal, I/O-free ChicagoHS biometric backend. These sources are compiled
# into libfprint's private driver library and do not form a public ABI.
goodix_chicago_sources = files(
    'goodix-chicago-calibration.c',
    'goodix-chicago-preprocess.c',
    'goodix-chicago-feature.c',
    'goodix-chicago-enrollment.c',
    'goodix-chicago-late-rejection-compat.c',
    'goodix-chicago-late-rejection.c',
    'goodix-chicago-match.c',
    'goodix-chicago-runtime.c',
    'goodix-chicago-bir.c',
    'goodix-chicago-template.c',
)

goodix_chicago_inc = include_directories('.')
