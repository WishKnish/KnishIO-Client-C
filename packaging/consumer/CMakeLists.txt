# find_package consumer of an installed KnishIO C SDK package, built by
# packaging/check-release-package.sh with -DCMAKE_PREFIX_PATH=<package> -DEXPECTED_VERSION=<ver>.
cmake_minimum_required(VERSION 3.20)
project(knishio_consumer C)

find_package(KnishIOClient REQUIRED)
if(NOT KnishIOClient_VERSION VERSION_EQUAL EXPECTED_VERSION)
    message(FATAL_ERROR
        "KnishIOClient ${KnishIOClient_VERSION} found at ${KnishIOClient_DIR}, expected ${EXPECTED_VERSION}")
endif()

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE KnishIO::knishio-client)

add_executable(consumer_static main.c)
target_link_libraries(consumer_static PRIVATE KnishIO::knishio-client-static)
