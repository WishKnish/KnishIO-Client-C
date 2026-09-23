# Runs ${EXE} --filter=no_such_suite and requires BOTH a non-zero exit status AND the runner's
# no-match line. WILL_FAIL alone also passes on a crash or a knishio_init() failure, and
# PASS_REGULAR_EXPRESSION alone ignores the exit status, so a runner that printed the line and
# still exited 0 (the defect this guards against) would pass.
if(NOT EXE)
    message(FATAL_ERROR "expect_filter_no_match_fails.cmake requires -DEXE=<path to knishio_tests>")
endif()

execute_process(COMMAND "${EXE}" --filter=no_such_suite
                OUTPUT_VARIABLE OUT ERROR_VARIABLE ERR RESULT_VARIABLE RC)

if(RC STREQUAL "0")
    message(FATAL_ERROR "knishio_tests --filter=no_such_suite exited 0: a filter that matches no "
                        "suite must fail\n${OUT}")
endif()

string(FIND "${OUT}" "No test suite matched --filter=no_such_suite" POS)
if(POS EQUAL -1)
    message(FATAL_ERROR "knishio_tests --filter=no_such_suite exited '${RC}' without the no-match "
                        "line (crash or knishio_init() failure?)\n${OUT}${ERR}")
endif()

message(STATUS "OK: a filter matching no suite exits ${RC} with the no-match line")
