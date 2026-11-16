add_library(openkal_terrain
  src/Floor.cpp
  src/IndoorCollision.cpp
  src/EnvLoader.cpp
  src/GbModel.cpp
  src/GtxLoader.cpp
  src/HeightField.cpp
  src/HeightPatch.cpp
  src/OplLoader.cpp
  src/PngLoader.cpp
)
add_library(openkal::terrain ALIAS openkal_terrain)

target_include_directories(openkal_terrain
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/engines/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(openkal_terrain PUBLIC openkal::map openkal::platform)
target_link_libraries(openkal_terrain PRIVATE PNG::PNG)
target_compile_features(openkal_terrain PUBLIC cxx_std_17)
