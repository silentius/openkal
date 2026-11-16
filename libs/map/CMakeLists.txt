add_library(openkal_map
  src/AreaCell.cpp
  src/AreaMapCodec.cpp
  src/WorldGrid.cpp
  src/MapCatalog.cpp
  src/IndoorMap.cpp
  src/MapStream.cpp
  src/MiniMapSize.cpp
  src/Portal.cpp
)
add_library(openkal::map ALIAS openkal_map)

target_include_directories(openkal_map
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(openkal_map PUBLIC openkal::platform)
target_compile_features(openkal_map PUBLIC cxx_std_17)
