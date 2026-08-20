if(NOT ATS_RUNNER OR NOT EXISTS "${ATS_RUNNER}")
    message(FATAL_ERROR "The source-linked AlongTrack runner was not built")
endif()

execute_process(
    COMMAND "${ATS_RUNNER}"
    RESULT_VARIABLE runner_status
    OUTPUT_VARIABLE runner_output
    ERROR_VARIABLE runner_error
)

if(NOT runner_status EQUAL 2)
    message(FATAL_ERROR
        "The source-linked runner usage path returned ${runner_status}, expected 2")
endif()
if(NOT runner_error MATCHES
   "\"stage\":\"arguments\".*\"message\":\"INPUT is required\\.\"")
    message(FATAL_ERROR
        "The source-linked runner did not reach the reusable runner entry point: ${runner_error}")
endif()
if(runner_output)
    message(FATAL_ERROR
        "The source-linked runner usage path unexpectedly wrote stdout: ${runner_output}")
endif()
