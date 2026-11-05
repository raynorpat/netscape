# Keep installation and every mutated fixture copy inside this build directory.
set(prefix "${BUILD_DIR}/tls installed 100%")
execute_process(COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}"
 --config "${CONFIG}" --prefix "${prefix}" --component Runtime
 RESULT_VARIABLE result)
if(NOT result EQUAL 0)
 message(FATAL_ERROR "TLS Runtime install failed: ${result}")
endif()
execute_process(COMMAND "${PYTHON}" "${CHECKER}"
 --executable "${prefix}/bin/netscape.exe"
 --output "${BUILD_DIR}/tls-installed" --mode installed RESULT_VARIABLE result)
if(NOT result EQUAL 0)
 message(FATAL_ERROR "Installed TLS workflow failed: ${result}")
endif()
