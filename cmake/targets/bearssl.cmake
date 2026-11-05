# Pinned upstream dependency; never download code during configuration.
set(bearssl_root "${PROJECT_SOURCE_DIR}/third_party/bearssl/upstream")
file(GLOB_RECURSE bearssl_sources CONFIGURE_DEPENDS "${bearssl_root}/src/*.c")
add_library(netscape_bearssl STATIC ${bearssl_sources})
set_target_properties(netscape_bearssl PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
target_include_directories(netscape_bearssl PUBLIC "${bearssl_root}/inc"
 PRIVATE "${bearssl_root}/src")
# The unmodified upstream system RNG uses the Windows CryptoAPI.
target_link_libraries(netscape_bearssl PRIVATE advapi32)

# The transport adapter owns trust stores and TLS state; upstream stays pristine.
add_library(netscape_tls STATIC "${PROJECT_SOURCE_DIR}/security/tls/net_tls_trust.c"
 "${PROJECT_SOURCE_DIR}/security/tls/net_tls_engine.c"
 "${PROJECT_SOURCE_DIR}/security/tls/net_tls_socket.c")
set_target_properties(netscape_tls PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
target_include_directories(netscape_tls PUBLIC "${PROJECT_SOURCE_DIR}/security/tls"
 "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(netscape_tls PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(netscape_tls PRIVATE netscape_bearssl)
install(FILES "${PROJECT_SOURCE_DIR}/security/ca/cacert.pem"
 DESTINATION share/netscape/ca COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/security/ca/README.md"
 DESTINATION share/doc/netscape RENAME CA-bundle-provenance.md COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/security/ca/LICENSE-MPL-2.0.txt"
 DESTINATION share/doc/netscape COMPONENT Runtime)

install(FILES "${PROJECT_SOURCE_DIR}/third_party/bearssl/README.md"
 DESTINATION share/doc/netscape RENAME BearSSL-provenance.md COMPONENT Runtime)
install(FILES "${PROJECT_SOURCE_DIR}/third_party/bearssl/upstream/LICENSE.txt"
 DESTINATION share/doc/netscape/licenses RENAME BearSSL-MIT.txt COMPONENT Runtime)
