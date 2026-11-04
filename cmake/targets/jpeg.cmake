# Source list from jpeg/Makefile; this tree builds the decoder subset.
add_library(netscape_jpeg STATIC
  ${PROJECT_SOURCE_DIR}/jpeg/jdapimin.c ${PROJECT_SOURCE_DIR}/jpeg/jdapistd.c
  ${PROJECT_SOURCE_DIR}/jpeg/jdatasrc.c ${PROJECT_SOURCE_DIR}/jpeg/jdmaster.c
  ${PROJECT_SOURCE_DIR}/jpeg/jdinput.c ${PROJECT_SOURCE_DIR}/jpeg/jdmarker.c
  ${PROJECT_SOURCE_DIR}/jpeg/jdhuff.c ${PROJECT_SOURCE_DIR}/jpeg/jdphuff.c
  ${PROJECT_SOURCE_DIR}/jpeg/jdmainct.c ${PROJECT_SOURCE_DIR}/jpeg/jdcoefct.c
  ${PROJECT_SOURCE_DIR}/jpeg/jdpostct.c ${PROJECT_SOURCE_DIR}/jpeg/jddctmgr.c
  ${PROJECT_SOURCE_DIR}/jpeg/jidctfst.c ${PROJECT_SOURCE_DIR}/jpeg/jidctflt.c
  ${PROJECT_SOURCE_DIR}/jpeg/jidctint.c ${PROJECT_SOURCE_DIR}/jpeg/jidctred.c
  ${PROJECT_SOURCE_DIR}/jpeg/jdsample.c ${PROJECT_SOURCE_DIR}/jpeg/jdcolor.c
  ${PROJECT_SOURCE_DIR}/jpeg/jquant1.c ${PROJECT_SOURCE_DIR}/jpeg/jquant2.c
  ${PROJECT_SOURCE_DIR}/jpeg/jdmerge.c ${PROJECT_SOURCE_DIR}/jpeg/jcomapi.c
  ${PROJECT_SOURCE_DIR}/jpeg/jutils.c ${PROJECT_SOURCE_DIR}/jpeg/jerror.c
  ${PROJECT_SOURCE_DIR}/jpeg/jmemmgr.c ${PROJECT_SOURCE_DIR}/jpeg/jmemnobs.c)
target_include_directories(netscape_jpeg PUBLIC ${PROJECT_SOURCE_DIR}/jpeg)
set_target_properties(netscape_jpeg PROPERTIES OUTPUT_NAME jpeg)
