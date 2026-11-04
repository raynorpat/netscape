sun_java_program(netscape_javap javap
  ${SUN_JAVA_STAGE}/javap/javap.c
  ${SUN_JAVA_STAGE}/runtime/check_class.c
  ${SUN_JAVA_STAGE}/runtime/check_code.c
  ${SUN_JAVA_STAGE}/runtime/decode.c
  ${SUN_JAVA_STAGE}/runtime/classloader.c
  ${SUN_JAVA_STAGE}/runtime/classresolver.c
  ${SUN_JAVA_STAGE}/runtime/classresolver_stubs.c
  ${SUN_JAVA_STAGE}/runtime/globals.c
  ${SUN_JAVA_STAGE}/runtime/runtime_stubs.c
  ${SUN_JAVA_STAGE}/runtime/utf.c
  ${SUN_JAVA_STAGE}/runtime/util.c
  ${SUN_JAVA_STAGE}/runtime/zip.c
  ${SUN_JAVA_STAGE}/md/path_md.c
  ${SUN_JAVA_STAGE}/md/system_md.c
)
target_link_libraries(netscape_javap PRIVATE netscape_nspr m ${CMAKE_DL_LIBS})
target_sources(netscape_javap PRIVATE "${PROJECT_SOURCE_DIR}/cmake/modules/sun_java_console.c")
