add_library(openkal_client_settings
  src/ClientSettings.cpp
)
add_library(openkal::settings ALIAS openkal_client_settings)

target_include_directories(openkal_client_settings
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(openkal_client_settings PUBLIC openkal::platform)
target_compile_features(openkal_client_settings PUBLIC cxx_std_17)
openkal_target_warnings(openkal_client_settings)
