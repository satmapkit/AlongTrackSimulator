file(GLOB_RECURSE portable_sources
    "${ATS_SOURCE_DIR}/cpp/include/*.h"
    "${ATS_SOURCE_DIR}/cpp/include/*.hpp"
    "${ATS_SOURCE_DIR}/cpp/src/*.c"
    "${ATS_SOURCE_DIR}/cpp/src/*.cc"
    "${ATS_SOURCE_DIR}/cpp/src/*.cpp"
)

foreach(source IN LISTS portable_sources)
    file(READ "${source}" contents)
    if(contents MATCHES "WaveVortexModel|WVModel|WVObserver|NetCDF|netcdf|observer registration|output routing")
        message(FATAL_ERROR "Portable source ${source} contains a forbidden runtime dependency")
    endif()
endforeach()

file(GLOB_RECURSE distributed_binaries
    "${ATS_SOURCE_DIR}/*.a"
    "${ATS_SOURCE_DIR}/*.dylib"
    "${ATS_SOURCE_DIR}/*.dll"
    "${ATS_SOURCE_DIR}/*.mex*"
    "${ATS_SOURCE_DIR}/*.o"
    "${ATS_SOURCE_DIR}/*.so"
)
if(distributed_binaries)
    list(JOIN distributed_binaries "\n  " binary_list)
    message(FATAL_ERROR "Compiled artifacts must not be distributed:\n  ${binary_list}")
endif()
