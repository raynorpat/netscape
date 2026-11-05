enable_language(CXX)
enable_language(RC)
set(CMAKE_MFC_FLAG 2)
# This frontend returns writable, request-owned about: buffers. Limit the
# release contract to these two consumers; other platform frontends differ.
set_property(SOURCE "${PROJECT_SOURCE_DIR}/lib/libnet/mkgeturl.c"
 "${PROJECT_SOURCE_DIR}/lib/libmsg/spool.c" APPEND PROPERTY COMPILE_DEFINITIONS WFE_OWNED_ABOUT_DATA)

add_executable(netscape-win32 WIN32
 "${PROJECT_SOURCE_DIR}/cmd/winfe/main.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/classic.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/chrome.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/mailui.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/find.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/browser-links.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/applications.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/frame-divider.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/printing.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/frontend.c"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/platform.c"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/services.c"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/composer.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/mailsettings.c"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/preferences-settings.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/preferences-ui.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/images.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/forms.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/downloads.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/history.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/bookmarks.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/netscape.rc"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/legacy/cmdparse.cpp"
 "${PROJECT_SOURCE_DIR}/cmd/winfe/legacy/genedit.cpp")
if(NETSCAPE_ENABLE_BEARSSL_HTTPS)
 target_sources(netscape-win32 PRIVATE "${PROJECT_SOURCE_DIR}/cmd/winfe/tls-platform.c")
 target_link_libraries(netscape-win32 PRIVATE netscape_tls bcrypt)
 add_custom_command(TARGET netscape-win32 POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:netscape-win32>/../share/netscape/ca"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PROJECT_SOURCE_DIR}/security/ca/cacert.pem"
   "$<TARGET_FILE_DIR:netscape-win32>/../share/netscape/ca/cacert.pem")
endif()
if(NETSCAPE_BUILD_WINDOWS_EDITOR)
 target_sources(netscape-win32 PRIVATE
  "${PROJECT_SOURCE_DIR}/cmd/winfe/editor-platform.cpp"
  "${PROJECT_SOURCE_DIR}/cmd/winfe/editor-ui.cpp"
  "${PROJECT_SOURCE_DIR}/cmd/winfe/editor-create.cpp"
  "${PROJECT_SOURCE_DIR}/cmd/winfe/editor-preferences.cpp"
  "${PROJECT_SOURCE_DIR}/cmd/winfe/editor-publish.cpp")
 if(BUILD_TESTING)
  target_sources(netscape-win32 PRIVATE "${PROJECT_SOURCE_DIR}/cmd/winfe/editor-stream-smoke.cpp")
  target_compile_definitions(netscape-win32 PRIVATE WFE_EDITOR_STREAM_TEST)
 endif()
endif()

target_include_directories(netscape-win32 PRIVATE "${PROJECT_SOURCE_DIR}/cmd/winfe")
target_compile_definitions(netscape-win32 PRIVATE _AFXDLL _MBCS)
set_target_properties(netscape-win32 PROPERTIES OUTPUT_NAME netscape CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
target_link_libraries(netscape-win32 PRIVATE netscape_platform
 netscape_xlate netscape_net netscape_lay netscape_i18n netscape_img netscape_jpeg
 netscape_misc netscape_parse netscape_msg netscape_mime netscape_plugin
 netscape_dbm netscape_hash netscape_xp netscape_nspr comctl32 shell32 shlwapi msimg32 comdlg32)
install(TARGETS netscape-win32 RUNTIME DESTINATION bin COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/cmd/winfe/legacy/LICENSE"
 DESTINATION share/doc/netscape/licenses RENAME WinFE-NPL-1.0 COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/cmd/winfe/legacy/README.md"
 DESTINATION share/doc/netscape RENAME Windows-source-provenance.md COMPONENT Runtime)
if(NETSCAPE_BUILD_WINDOWS_EDITOR)
 install(FILES "${PROJECT_SOURCE_DIR}/cmd/winfe/res/gold/README.md"
  DESTINATION share/doc/netscape RENAME Gold-resource-provenance.md COMPONENT Runtime)
endif()
install(FILES "${PROJECT_SOURCE_DIR}/cmake/windows.md"
 "${PROJECT_SOURCE_DIR}/cmake/windows-roadmap.md"
 "${PROJECT_SOURCE_DIR}/cmake/windows-reference.md"
 DESTINATION share/doc/netscape COMPONENT Runtime)
if(BUILD_TESTING)
 add_test(NAME windows_frontend_local COMMAND netscape-win32 --smoke-test "${PROJECT_BINARY_DIR}/frontend-local.txt")
 set_tests_properties(windows_frontend_local PROPERTIES TIMEOUT 25
  ENVIRONMENT "NETSCAPE_PROFILE_DIR=${PROJECT_BINARY_DIR}/frontend-local-profile")
 find_package(Python3 REQUIRED COMPONENTS Interpreter)
 add_test(NAME windows_frontend_http COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_frontend.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/frontend-http"
  --edition "$<IF:$<BOOL:${NETSCAPE_BUILD_WINDOWS_EDITOR}>,gold,standard>")
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
 add_test(NAME windows_browser_images COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_browser_features.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/browser-images-test" --mode images)
 add_test(NAME windows_browser_forms COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_browser_features.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/browser-forms-test" --mode forms)
 set_tests_properties(windows_browser_images windows_browser_forms PROPERTIES TIMEOUT 100)
 add_test(NAME windows_browser_frames COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_frames.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/browser-frames-test")
 set_tests_properties(windows_browser_frames PROPERTIES TIMEOUT 60)
 add_test(NAME windows_downloads COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_downloads.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/downloads-test")
 set_tests_properties(windows_downloads PROPERTIES TIMEOUT 60)
 add_test(NAME windows_browser_history COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_history.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/history-test")
 set_tests_properties(windows_browser_history PROPERTIES TIMEOUT 60)
 add_test(NAME windows_bookmarks COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_bookmarks.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/bookmarks-test")
 set_tests_properties(windows_bookmarks PROPERTIES TIMEOUT 80)
 add_test(NAME windows_classic_chrome COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_chrome.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/chrome-test"
  --edition "$<IF:$<BOOL:${NETSCAPE_BUILD_WINDOWS_EDITOR}>,gold,standard>")
 set_tests_properties(windows_classic_chrome PROPERTIES TIMEOUT 60)
 add_test(NAME windows_printing COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_printing.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/printing-test")
 set_tests_properties(windows_printing PROPERTIES TIMEOUT 80)
 add_test(NAME windows_browser_interaction COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_interaction.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/interaction-test")
 set_tests_properties(windows_browser_interaction PROPERTIES TIMEOUT 90)
 add_test(NAME windows_browser_links COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_interaction.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/links-test" --mode links)
 set_tests_properties(windows_browser_links PROPERTIES TIMEOUT 90)
 add_test(NAME windows_frame_dividers COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_interaction.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/dividers-test" --mode dividers)
 set_tests_properties(windows_frame_dividers PROPERTIES TIMEOUT 90)
endif()

if(BUILD_TESTING AND NETSCAPE_BUILD_WINDOWS_EDITOR)
 add_test(NAME windows_gold_workflows COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_editor_workflows.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/workflows-test")
 add_test(NAME windows_gold_publishing COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_editor_publish.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/publish-test")
 set_tests_properties(windows_gold_workflows PROPERTIES TIMEOUT 150)
 set_tests_properties(windows_gold_publishing PROPERTIES TIMEOUT 160)
 add_test(NAME windows_gold_editor COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_editor.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/editor-test")
 set_tests_properties(windows_gold_editor PROPERTIES TIMEOUT 110)
endif()

if(BUILD_TESTING)
 add_test(NAME windows_mail_layout COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_mail_ui.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/mail-layout-test" --mode mail)
 set_tests_properties(windows_mail_layout PROPERTIES TIMEOUT 60)
 add_test(NAME windows_composition_ui COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_mail_ui.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/composition-ui-test" --mode compose)
 set_tests_properties(windows_composition_ui PROPERTIES TIMEOUT 60)
endif()

if(BUILD_TESTING)
 add_test(NAME windows_news_layout COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_news_ui.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/news-layout-test")
 set_tests_properties(windows_news_layout PROPERTIES TIMEOUT 65)
endif()

if(BUILD_TESTING)
 add_test(NAME windows_preferences COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_preferences.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/preferences-test")
 set_tests_properties(windows_preferences PROPERTIES TIMEOUT 140)
endif()

if(BUILD_TESTING)
 add_test(NAME windows_background_preferences COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_background.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/background-test")
 set_tests_properties(windows_background_preferences PROPERTIES TIMEOUT 170)
endif()

if(BUILD_TESTING)
 add_executable(netscape_windows_application_probe WIN32 "${PROJECT_SOURCE_DIR}/cmake/tests/windows_application_probe.c")
 set_property(TARGET netscape_windows_application_probe PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 add_test(NAME windows_supporting_applications COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_applications.py"
  --executable "$<TARGET_FILE:netscape-win32>" --probe "$<TARGET_FILE:netscape_windows_application_probe>"
  --output "${PROJECT_BINARY_DIR}/applications-test")
 set_tests_properties(windows_supporting_applications PROPERTIES TIMEOUT 230)
endif()

if(BUILD_TESTING AND NETSCAPE_ENABLE_BEARSSL_HTTPS)
 add_test(NAME windows_browser_tls COMMAND "${Python3_EXECUTABLE}"
  "${PROJECT_SOURCE_DIR}/cmake/tests/check_windows_tls.py"
  --executable "$<TARGET_FILE:netscape-win32>" --output "${PROJECT_BINARY_DIR}/tls-browser"
  --mode browser)
 set_tests_properties(windows_browser_tls PROPERTIES TIMEOUT 150)
endif()
