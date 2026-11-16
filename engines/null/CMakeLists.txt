add_library(openkal_engine_null
  null_engine.cpp
  ../TransparentMesh.cpp
)
add_library(openkal::engine ALIAS openkal_engine_null)

target_include_directories(openkal_engine_null
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(openkal_engine_null PUBLIC cxx_std_17)
