sun_java_program(netscape_javah javah
  ${SUN_JAVA_STAGE}/javah/javah.c
  ${SUN_JAVA_STAGE}/runtime/classloader.c
  ${SUN_JAVA_STAGE}/runtime/classresolver.c
  ${SUN_JAVA_STAGE}/runtime/classresolver_stubs.c
  ${SUN_JAVA_STAGE}/runtime/globals.c
  ${SUN_JAVA_STAGE}/runtime/runtime_stubs.c
  ${SUN_JAVA_STAGE}/runtime/utf.c
  ${SUN_JAVA_STAGE}/runtime/util.c
  ${SUN_JAVA_STAGE}/runtime/zip.c
  ${SUN_JAVA_STAGE}/md/path_md.c
  ${SUN_JAVA_STAGE}/md/stubs_md.c
  ${SUN_JAVA_STAGE}/md/system_md.c
)
target_link_libraries(netscape_javah PRIVATE netscape_nspr m ${CMAKE_DL_LIBS})
