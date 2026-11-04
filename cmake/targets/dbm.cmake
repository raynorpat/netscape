# Legacy database library, from lib/libdbm/Makefile.
add_library(netscape_dbm STATIC
  ${PROJECT_SOURCE_DIR}/lib/libdbm/db.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/h_bigkey.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/h_func.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/h_log2.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/h_page.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/hash.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/hash_buf.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/hsearch.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/memmove.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/mktemp.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/ndbm.c
  ${PROJECT_SOURCE_DIR}/lib/libdbm/snprintf.c
)
target_include_directories(netscape_dbm PRIVATE ${PROJECT_SOURCE_DIR}/lib/libdbm ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(netscape_dbm PRIVATE MEMMOVE __DBINTERFACE_PRIVATE)
set_target_properties(netscape_dbm PROPERTIES OUTPUT_NAME dbm)
add_dependencies(netscape_dbm netscape_nspr_headers)
target_include_directories(netscape_dbm PRIVATE ${PROJECT_SOURCE_DIR}/nspr/include ${NSPR_GENERATED_INCLUDE})
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  target_compile_definitions(netscape_dbm PRIVATE IS_64)
endif()
