# Project-owned registration data for out-of-tree Goodix SPI drivers.
# Adding a driver should normally require changing this file, the matching
# libfprint source fragment, and the supported-device metadata—not the pinned
# upstream patch.
goodix_spi_driver_names = [
    'gdix51c0',
]

goodix_spi_driver_helper_mapping = {
    'gdix51c0' : [ 'udev', 'openssl', 'gpiod' ],
}

if get_option('goodix_spi_developer')
    add_project_arguments('-DGOODIX_SPI_DEVELOPER=1', language: 'c')
    message('Goodix SPI developer diagnostics and fault injection enabled')
endif
