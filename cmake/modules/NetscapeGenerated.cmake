include_guard(GLOBAL)
set(generator "${PROJECT_SOURCE_DIR}/cmake/modules/generate.py")
file(MAKE_DIRECTORY "${NETSCAPE_GENERATED}/net" "${NETSCAPE_GENERATED}/xfe")
add_custom_command(OUTPUT "${NETSCAPE_GENERATED}/net/mktypes.h"
 COMMAND ${Python3_EXECUTABLE} "${generator}" mime "${PROJECT_SOURCE_DIR}" "${NETSCAPE_GENERATED}/net/mktypes.h"
 DEPENDS "${generator}" "${PROJECT_SOURCE_DIR}/cmd/xfe/ad2c" "${PROJECT_SOURCE_DIR}/lib/libnet/mime.types" "${PROJECT_SOURCE_DIR}/lib/libnet/mime.types-unix" "${PROJECT_SOURCE_DIR}/lib/libnet/mime.types-nonunix" VERBATIM)
# A quoted include searches the C file's directory first. Copy this translation
# unit so the checked-in legacy header cannot shadow the generated header.
configure_file("${PROJECT_SOURCE_DIR}/lib/libnet/mkformat.c" "${NETSCAPE_GENERATED}/net/mkformat.c" COPYONLY)
target_sources(netscape_net PRIVATE "${NETSCAPE_GENERATED}/net/mkformat.c" "${NETSCAPE_GENERATED}/net/mktypes.h")
if(NETSCAPE_ENABLE_JAVA)
 file(READ "${PROJECT_SOURCE_DIR}/build/build_number" build_number)
 string(STRIP "${build_number}" build_number)
 file(WRITE "${NETSCAPE_GENERATED}/ljbuild.h" "static int build_number = ${build_number};\n")
 set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/build/build_number")
 file(STRINGS "${PROJECT_SOURCE_DIR}/cmd/xfe/versionn.h" version_line REGEX "^#define VERSION_NUMBER")
 string(REGEX REPLACE "^#define VERSION_NUMBER[ \t]+" "" java_version "${version_line}")
 target_compile_definitions(netscape_java PRIVATE ZIP_NAME="java_${java_version}")
endif()
