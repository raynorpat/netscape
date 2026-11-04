enable_language(CXX)
enable_language(RC)
set(CMAKE_MFC_FLAG 2)
add_executable(netscape-win32 WIN32
 "${PROJECT_SOURCE_DIR}/cmd/winfe/main.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/frontend.c"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/platform.c"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/services.c"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/composer.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/mailsettings.c"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/netscape.rc"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/legacy/cmdparse.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/legacy/genedit.cpp")
target_include_directories(netscape-win32 PRIVATE "${PROJECT_SOURCE_DIR}/cmd/winfe")
target_compile_definitions(netscape-win32 PRIVATE _AFXDLL _MBCS)
set_target_properties(netscape-win32 PROPERTIES OUTPUT_NAME netscape CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(netscape-win32 PRIVATE netscape_platform
 netscape_xlate netscape_net netscape_lay netscape_i18n netscape_img netscape_jpeg
 netscape_misc netscape_parse netscape_msg netscape_mime netscape_plugin
 netscape_dbm netscape_hash netscape_xp netscape_nspr comctl32 shell32 shlwapi)
install(TARGETS netscape-win32 RUNTIME DESTINATION bin COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/cmd/winfe/legacy/LICENSE"
 DESTINATION share/doc/netscape/licenses RENAME WinFE-NPL-1.0 COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/cmd/winfe/legacy/README.md"
 DESTINATION share/doc/netscape RENAME Windows-source-provenance.md COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/cmake/windows.md"
 "${PROJECT_SOURCE_DIR}/cmake/windows-roadmap.md"
 DESTINATION share/doc/netscape COMPONENT Runtime)
if(BUILD_TESTING)
 add_test(NAME windows_frontend_local COMMAND netscape-win32 --smoke-test "${PROJECT_BINARY_DIR}/frontend-local.txt")
 set_tests_properties(windows_frontend_local PROPERTIES TIMEOUT 25
  ENVIRONMENT "NETSCAPE_PROFILE_DIR=${PROJECT_BINARY_DIR}/frontend-local-profile")
 find_package(Python3 REQUIRED COMPONENTS Interpreter)
 add_test(NAME windows_frontend_http COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_frontend.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/frontend-http")
 set_tests_properties(windows_frontend_http PROPERTIES TIMEOUT 40)
 add_test(NAME windows_mailnews COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_mailnews.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/mailnews-test"
  --mode all --delivery-fixtures)
 set_tests_properties(windows_mailnews PROPERTIES TIMEOUT 120)
 add_test(NAME windows_news_empty COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_mailnews.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/news-empty-test"
  --mode empty-news)
 set_tests_properties(windows_news_empty PROPERTIES TIMEOUT 40)
endif()
