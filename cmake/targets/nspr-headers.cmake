if(CMAKE_CROSSCOMPILING)
  message(FATAL_ERROR "NSPR CPU configuration requires a native build")
endif()
add_executable(netscape_gencfg EXCLUDE_FROM_ALL ${PROJECT_SOURCE_DIR}/nspr/include/gencfg.c)
set(NSPR_GENERATED_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/generated/nspr)
file(MAKE_DIRECTORY "${NSPR_GENERATED_INCLUDE}")
add_custom_command(OUTPUT "${NSPR_GENERATED_INCLUDE}/prcpucfg.h"
  COMMAND ${CMAKE_COMMAND} -DGENERATOR=$<TARGET_FILE:netscape_gencfg> -DOUTPUT=${NSPR_GENERATED_INCLUDE}/prcpucfg.h -P ${PROJECT_SOURCE_DIR}/cmake/modules/RunGenerator.cmake
  DEPENDS netscape_gencfg "${PROJECT_SOURCE_DIR}/cmake/modules/RunGenerator.cmake" VERBATIM)
add_custom_target(netscape_nspr_headers DEPENDS "${NSPR_GENERATED_INCLUDE}/prcpucfg.h")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(netscape_gencfg PRIVATE LINUX)
endif()
