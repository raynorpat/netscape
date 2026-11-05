include(NetscapeWindows)
include("${CMAKE_CURRENT_LIST_DIR}/nspr.cmake")
target_link_libraries(netscape_dbm PRIVATE netscape_platform)
# The UCRT provides these functions, unlike the original Windows CRT.
get_target_property(dbm_sources netscape_dbm SOURCES)
list(REMOVE_ITEM dbm_sources "${PROJECT_SOURCE_DIR}/lib/libdbm/snprintf.c")
set_property(TARGET netscape_dbm PROPERTY SOURCES "${dbm_sources}")
# Declare the backend archives independently of the unavailable 3.02 WinFE.
# Their Windows source compatibility is evaluated separately from NSPR.
foreach(component parse lay img i18n misc msg net plugin xlate xp mime hash)
 include("${PROJECT_SOURCE_DIR}/cmake/targets/${component}.cmake")
endforeach()
if(NETSCAPE_ENABLE_BEARSSL_HTTPS)
 target_link_libraries(netscape_net PRIVATE netscape_tls)
endif()
# netscape_archive normally includes its source directory; use exported headers
# for netlib too so <winsock.h> is supplied by the SDK.
get_target_property(net_includes netscape_net INCLUDE_DIRECTORIES)
list(REMOVE_ITEM net_includes "${PROJECT_SOURCE_DIR}/lib/libnet")
set_property(TARGET netscape_net PROPERTY INCLUDE_DIRECTORIES "${net_includes}")
# Linux's generated parser sources are replaced by the checked-in tables until
# native gperf/security resource tools are wired into the Windows build.
target_sources(netscape_parse PRIVATE "${PROJECT_SOURCE_DIR}/lib/libparse/pa_hash.c" "${PROJECT_SOURCE_DIR}/lib/libparse/pa_mdl.c")
target_include_directories(netscape_i18n PRIVATE "${PROJECT_SOURCE_DIR}/lib/libi18n/unicode/ufrmtbl" "${PROJECT_SOURCE_DIR}/lib/libi18n/unicode/utotbl")
target_compile_definitions(netscape_i18n PRIVATE HAS_FE)
target_compile_definitions(netscape_misc PRIVATE HAS_FE)
target_link_libraries(netscape_img PUBLIC netscape_jpeg)
get_target_property(net_sources netscape_net SOURCES)
# Unix process-based converters and DNS are replaced by WinFE services.
foreach(source unix-dns.c cvextcon.c cvview.c)
 list(REMOVE_ITEM net_sources "${PROJECT_SOURCE_DIR}/lib/libnet/${source}")
endforeach()
set_property(TARGET netscape_net PROPERTY SOURCES "${net_sources}")
target_sources(netscape_net PRIVATE "${PROJECT_SOURCE_DIR}/lib/libnet/mkformat.c")
# The Windows frontend supplies localized strings and plugin loading.
get_target_property(xp_sources netscape_xp SOURCES)
list(REMOVE_ITEM xp_sources "${PROJECT_SOURCE_DIR}/lib/xp/xp_intl.c")
set_property(TARGET netscape_xp PROPERTY SOURCES "${xp_sources}")
get_target_property(plugin_sources netscape_plugin SOURCES)
list(REMOVE_ITEM plugin_sources "${PROJECT_SOURCE_DIR}/lib/plugin/upi.c" "${PROJECT_SOURCE_DIR}/lib/plugin/testupi.c")
set_property(TARGET netscape_plugin PROPERTY SOURCES "${plugin_sources}")
set(native_core netscape_nspr netscape_dbm netscape_jpeg netscape_hash netscape_parse netscape_lay netscape_img netscape_i18n netscape_misc netscape_msg netscape_net netscape_plugin netscape_xlate netscape_xp netscape_mime)
add_custom_target(netscape_windows_core DEPENDS ${native_core})
include(CTest)
if(BUILD_TESTING)
 if(NETSCAPE_ENABLE_BEARSSL_HTTPS)
  add_executable(netscape_http_tls_post_test "${PROJECT_SOURCE_DIR}/cmake/tests/http_tls_post_test.c")
  target_compile_definitions(netscape_http_tls_post_test PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(netscape_http_tls_post_test PRIVATE ws2_32)
  foreach(case zero_error short_error eof data_eof)
   add_test(NAME http_tls_post_${case} COMMAND netscape_http_tls_post_test ${case})
   set_tests_properties(http_tls_post_${case} PROPERTIES TIMEOUT 20)
  endforeach()
  add_executable(netscape_tls_dependency_test "${PROJECT_SOURCE_DIR}/cmake/tests/tls_dependency_test.c")
  target_link_libraries(netscape_tls_dependency_test PRIVATE netscape_bearssl)
  add_executable(netscape_tls_engine_test "${PROJECT_SOURCE_DIR}/cmake/tests/tls_engine_test.c")
  target_link_libraries(netscape_tls_engine_test PRIVATE netscape_tls ws2_32)
  add_executable(netscape_tls_trust_test "${PROJECT_SOURCE_DIR}/cmake/tests/tls_trust_test.c")
  target_include_directories(netscape_tls_trust_test PRIVATE "${PROJECT_SOURCE_DIR}/security/tls" "${PROJECT_SOURCE_DIR}/include")
  target_link_libraries(netscape_tls_trust_test PRIVATE netscape_tls netscape_bearssl)
  foreach(case valid_bundle unknown_pem_object empty_bundle malformed_tail unsupported_anchor oversized_bundle ownership shipped_bundle)
   file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tls-trust/${case}")
   add_test(NAME tls_trust_${case} COMMAND netscape_tls_trust_test ${case}
    "${PROJECT_SOURCE_DIR}/cmake/tests/data/tls" "${PROJECT_SOURCE_DIR}/security/ca/cacert.pem")
   set_tests_properties(tls_trust_${case} PROPERTIES TIMEOUT 20 WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/tls-trust/${case}")
  endforeach()
  add_executable(netscape_tls_trust_alloc_test "${PROJECT_SOURCE_DIR}/cmake/tests/tls_trust_alloc_test.c")
  target_include_directories(netscape_tls_trust_alloc_test PRIVATE "${PROJECT_SOURCE_DIR}/security/tls" "${PROJECT_SOURCE_DIR}/include")
  target_compile_definitions(netscape_tls_trust_alloc_test PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(netscape_tls_trust_alloc_test PRIVATE netscape_bearssl)
  file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/tls-trust/allocation")
  add_test(NAME tls_trust_allocation COMMAND netscape_tls_trust_alloc_test "${PROJECT_SOURCE_DIR}/cmake/tests/data/tls")
  set_tests_properties(tls_trust_allocation PROPERTIES TIMEOUT 20 WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/tls-trust/allocation")
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_program(NETSCAPE_TLS_FIXTURE_OPENSSL NAMES openssl
   HINTS "$ENV{LOCALAPPDATA}/Programs/Git/mingw64/bin" REQUIRED)
  add_executable(netscape_tls_x509_test "${PROJECT_SOURCE_DIR}/cmake/tests/tls_x509_test.c")
  target_link_libraries(netscape_tls_x509_test PRIVATE netscape_tls netscape_bearssl)
  add_test(NAME tls_x509_purpose COMMAND "${Python3_EXECUTABLE}"
   "${PROJECT_SOURCE_DIR}/cmake/tests/tls_x509_fixture.py" --client "$<TARGET_FILE:netscape_tls_x509_test>")
  set_tests_properties(tls_x509_purpose PROPERTIES TIMEOUT 70)
  add_test(NAME tls_engine_prerequisites COMMAND "${Python3_EXECUTABLE}"
   "${PROJECT_SOURCE_DIR}/cmake/tests/tls_fixture.py"
   --openssl "${NETSCAPE_TLS_FIXTURE_OPENSSL}" --case prerequisites)
  set_tests_properties(tls_engine_prerequisites PROPERTIES FIXTURES_SETUP tls_peer TIMEOUT 40)
  foreach(case success_1 success_7 success_4096 mixed_case future fail_name fail_root fail_expired fail_future
    fail_identity fail_ipv4hex fail_ipv6 fail_nonascii fail_entropy fail_time fail_time_range fail_time_negative_range
    cancel close_pending renegotiate truncate alert malformed_alert fail_client_auth
    fail_tls10 fail_tls11 legacy10 legacy11 legacy_ec fail_forbidden fail_forbidden_legacy
    fail_purpose_client_rsa fail_purpose_client_ec fail_purpose_code_rsa
    fail_purpose_client_inter_rsa fail_purpose_code_inter_rsa
    fail_purpose_client_rsa_legacy fail_purpose_client_ec_legacy fail_purpose_code_rsa_legacy
    fail_purpose_client_inter_rsa_legacy fail_purpose_code_inter_rsa_legacy
    purpose_server_rsa purpose_absent_rsa purpose_mixed_rsa purpose_any_rsa
    purpose_server_rsa_legacy purpose_absent_rsa_legacy purpose_mixed_rsa_legacy purpose_any_rsa_legacy
    purpose_mixed_inter_rsa purpose_any_inter_rsa
    purpose_absent_ec purpose_mixed_ec purpose_any_ec
    purpose_absent_ec_legacy purpose_mixed_ec_legacy purpose_any_ec_legacy
    fail_purpose_other_rsa fail_purpose_other_rsa_legacy
    fail_purpose_critical_rsa fail_purpose_keyusage_rsa fail_purpose_unknown_rsa
    fail_purpose_critical_rsa_legacy fail_purpose_keyusage_rsa_legacy fail_purpose_unknown_rsa_legacy
    fail_default_cbc rsa128 rsa256 rsachacha ec128 ec256 ecchacha)
   add_test(NAME tls_engine_${case} COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/cmake/tests/tls_fixture.py"
    --openssl "${NETSCAPE_TLS_FIXTURE_OPENSSL}" --client "$<TARGET_FILE:netscape_tls_engine_test>" --case ${case})
   set_tests_properties(tls_engine_${case} PROPERTIES FIXTURES_REQUIRED tls_peer TIMEOUT 70)
  endforeach()
  add_test(NAME tls_dependency COMMAND netscape_tls_dependency_test)
  set_tests_properties(tls_dependency PROPERTIES TIMEOUT 20)
 endif()
 add_executable(netscape_windows_core_smoke "${PROJECT_SOURCE_DIR}/cmake/tests/windows_core_smoke.c")
 target_link_libraries(netscape_windows_core_smoke PRIVATE netscape_platform netscape_nspr netscape_dbm)
 add_test(NAME windows_core_smoke COMMAND netscape_windows_core_smoke)
 set_tests_properties(windows_core_smoke PROPERTIES TIMEOUT 20)
 add_executable(netscape_windows_socket_smoke "${PROJECT_SOURCE_DIR}/cmake/tests/windows_socket_smoke.c")
 target_link_libraries(netscape_windows_socket_smoke PRIVATE netscape_platform)
 if(NETSCAPE_ENABLE_BEARSSL_HTTPS)
  target_sources(netscape_windows_socket_smoke PRIVATE "${PROJECT_SOURCE_DIR}/cmd/winfe/tls-platform.c")
  target_link_libraries(netscape_windows_socket_smoke PRIVATE netscape_tls bcrypt)
 endif()
 add_test(NAME windows_socket_smoke COMMAND netscape_windows_socket_smoke)
 set_tests_properties(windows_socket_smoke PROPERTIES TIMEOUT 20)
endif()
if(BUILD_TESTING AND NETSCAPE_ENABLE_BEARSSL_HTTPS)
 add_executable(netscape_tls_socket_test "${PROJECT_SOURCE_DIR}/cmake/tests/tls_socket_test.c")
 target_link_libraries(netscape_tls_socket_test PRIVATE netscape_tls netscape_bearssl)
 add_test(NAME tls_socket COMMAND netscape_tls_socket_test "${PROJECT_SOURCE_DIR}/cmake/tests/data/tls/test-ca.pem")
 target_include_directories(netscape_tls_socket_test PRIVATE "${PROJECT_SOURCE_DIR}/third_party/bearssl/upstream/inc")
 add_executable(netscape_tls_platform_test "${PROJECT_SOURCE_DIR}/cmake/tests/tls_platform_test.c"
  "${PROJECT_SOURCE_DIR}/cmd/winfe/tls-platform.c")
 target_link_libraries(netscape_tls_platform_test PRIVATE netscape_tls ws2_32 bcrypt)
 add_custom_command(TARGET netscape_tls_platform_test POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:netscape_tls_platform_test>/../share/netscape/ca"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PROJECT_SOURCE_DIR}/security/ca/cacert.pem"
   "$<TARGET_FILE_DIR:netscape_tls_platform_test>/../share/netscape/ca/cacert.pem")
 add_custom_command(TARGET netscape_tls_platform_test POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/tls-platform-missing/bin"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:netscape_tls_platform_test>"
   "${PROJECT_BINARY_DIR}/tls-platform-missing/bin/tls-platform-test.exe")
 add_test(NAME tls_socket_platform_missing COMMAND "${PROJECT_BINARY_DIR}/tls-platform-missing/bin/tls-platform-test.exe" missing)
 set_tests_properties(tls_socket_platform_missing PROPERTIES TIMEOUT 20 WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/security/ca")
 add_test(NAME tls_socket_platform COMMAND netscape_tls_platform_test)
 foreach(case success_1 success_4096 fail_name fail_root)
  add_test(NAME tls_socket_peer_${case} COMMAND "${Python3_EXECUTABLE}"
   "${PROJECT_SOURCE_DIR}/cmake/tests/tls_fixture.py"
   --openssl "${NETSCAPE_TLS_FIXTURE_OPENSSL}" --client "$<TARGET_FILE:netscape_tls_platform_test>" --case ${case})
  set_tests_properties(tls_socket_peer_${case} PROPERTIES FIXTURES_REQUIRED tls_peer TIMEOUT 70)
 endforeach()
 set_tests_properties(tls_socket tls_socket_platform PROPERTIES TIMEOUT 20)
endif()
