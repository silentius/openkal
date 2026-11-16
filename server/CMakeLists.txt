find_package(OpenSSL 3.0 REQUIRED)

add_executable(openkal_mainsvr
  MainSvrMain.cpp
  ServerStartupConfig.cpp
  DevelopmentCertificate.cpp
)
add_dependencies(openkal_mainsvr openkal_config_archive)

target_link_libraries(openkal_mainsvr
  PRIVATE
    openkal::server
    openkal::platform
    OpenSSL::Crypto
)

target_compile_features(openkal_mainsvr PRIVATE cxx_std_17)
target_compile_definitions(openkal_mainsvr
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
)

if(NOT OPENKAL_RELEASE_BUILD AND NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_definitions(openkal_mainsvr PRIVATE
    OPENKAL_DEVELOPMENT_ROOT="${CMAKE_SOURCE_DIR}")
endif()
