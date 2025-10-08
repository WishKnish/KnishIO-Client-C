# Test suite for KnishIO C Client SDK

# Try to find Unity testing framework
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(UNITY unity)
endif()

if(NOT UNITY_FOUND)
    # Use FetchContent to download Unity if not found
    include(FetchContent)
    FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG v2.5.2
    )
    set(UNITY_EXTENSION_FIXTURE ON CACHE BOOL "Enable Unity fixtures")
    FetchContent_MakeAvailable(unity)
    set(UNITY_LIBRARIES unity)
    set(UNITY_INCLUDE_DIRS ${unity_SOURCE_DIR}/src)
endif()

# Test sources
set(TEST_SOURCES
    test_main.c
    test_memory.c
    test_string.c
    test_error.c
    test_shake256.c
    test_signatures.c  # WOTS+ quantum-resistant signature verification tests
    test_molecule.c    # Molecular validation and signing tests
    benchmark_init.c   # SDK initialization performance benchmarks
    test_memory_leaks.c # Systematic memory leak detection tests
    test_bigint.c
    test_wallet.c
)

# Create test executable
add_executable(knishio-tests ${TEST_SOURCES})

target_include_directories(knishio-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${UNITY_INCLUDE_DIRS}
)

target_link_libraries(knishio-tests PRIVATE
    knishio-client-static
    ${UNITY_LIBRARIES}
)

# Copy test vectors if they exist
if(EXISTS ${CMAKE_SOURCE_DIR}/../../../validation/test-vectors.json)
    configure_file(
        ${CMAKE_SOURCE_DIR}/../../../validation/test-vectors.json
        ${CMAKE_CURRENT_BINARY_DIR}/test-vectors.json
        COPYONLY
    )
endif()

# Individual test registration
foreach(test_source ${TEST_SOURCES})
    if(test_source MATCHES "test_([^.]+)\\.c")
        set(test_name ${CMAKE_MATCH_1})
        add_test(NAME ${test_name} COMMAND knishio-tests --filter=${test_name})
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 30)
    endif()
endforeach()

# Advanced memory leak detection with Valgrind
find_program(VALGRIND_PROGRAM valgrind)
if(VALGRIND_PROGRAM)
    message(STATUS "Valgrind found: ${VALGRIND_PROGRAM}")
    
    # Create Valgrind configuration
    set(VALGRIND_SUPPRESSIONS "${CMAKE_CURRENT_SOURCE_DIR}/../valgrind.supp")
    
    # Comprehensive memory leak test
    add_test(
        NAME memory_leaks_comprehensive
        COMMAND ${VALGRIND_PROGRAM} 
            --tool=memcheck 
            --leak-check=full
            --show-leak-kinds=all 
            --track-origins=yes 
            --verbose
            --error-exitcode=42
            --suppressions=${VALGRIND_SUPPRESSIONS}
            --log-file=${CMAKE_CURRENT_BINARY_DIR}/valgrind_comprehensive.log
            $<TARGET_FILE:knishio-tests> --filter=memory
    )
    set_tests_properties(memory_leaks_comprehensive PROPERTIES 
        TIMEOUT 300
        LABELS "memory;valgrind;comprehensive"
    )
    
    # Quick memory leak test (for regular CI)
    add_test(
        NAME memory_leaks_quick
        COMMAND ${VALGRIND_PROGRAM} 
            --tool=memcheck 
            --leak-check=full
            --error-exitcode=42
            --suppressions=${VALGRIND_SUPPRESSIONS}
            $<TARGET_FILE:knishio-tests> --filter=memory_sdk
    )
    set_tests_properties(memory_leaks_quick PROPERTIES 
        TIMEOUT 120
        LABELS "memory;valgrind;quick"
    )
    
    # Memory leak test for individual components
    foreach(component client wallet auth atom molecule)
        add_test(
            NAME memory_leaks_${component}
            COMMAND ${VALGRIND_PROGRAM} 
                --tool=memcheck 
                --leak-check=full
                --error-exitcode=42
                --suppressions=${VALGRIND_SUPPRESSIONS}
                $<TARGET_FILE:knishio-tests> --filter=memory_${component}
        )
        set_tests_properties(memory_leaks_${component} PROPERTIES 
            TIMEOUT 120
            LABELS "memory;valgrind;component"
        )
    endforeach()
    
    # Add custom target for running all memory tests
    add_custom_target(test_memory_all
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../run_memory_tests.sh
        DEPENDS knishio-tests
        COMMENT "Running comprehensive memory leak detection tests"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
else()
    message(WARNING "Valgrind not found - memory leak detection will be limited")
    
    # Basic memory tests without Valgrind
    add_test(NAME memory_basic COMMAND knishio-tests --filter=memory)
    set_tests_properties(memory_basic PROPERTIES TIMEOUT 60)
endif()

# Coverage reports (if gcov is available)
if(KNISHIO_ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    find_program(GCOV_PROGRAM gcov)
    if(GCOV_PROGRAM)
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory coverage
            COMMAND ${GCOV_PROGRAM} -r ${CMAKE_BINARY_DIR}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating coverage report"
        )
        add_dependencies(coverage knishio-tests)
    endif()
endif()

# Cross-SDK compatibility tests
add_test(
    NAME cross_sdk_compatibility
    COMMAND knishio-tests --filter=cross_sdk
)
set_tests_properties(cross_sdk_compatibility PROPERTIES TIMEOUT 60)