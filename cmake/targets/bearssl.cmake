# Pinned upstream dependency; never download code during configuration.
set(bearssl_root "${PROJECT_SOURCE_DIR}/third_party/bearssl/upstream")
file(GLOB_RECURSE bearssl_sources CONFIGURE_DEPENDS "${bearssl_root}/src/*.c")
add_library(netscape_bearssl STATIC ${bearssl_sources})
set_target_properties(netscape_bearssl PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
target_include_directories(netscape_bearssl PUBLIC "${bearssl_root}/inc"
 PRIVATE "${bearssl_root}/src")
# The unmodified upstream system RNG uses the Windows CryptoAPI.
target_link_libraries(netscape_bearssl PRIVATE advapi32)
