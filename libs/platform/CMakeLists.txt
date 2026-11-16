add_library(openkal_platform
  src/Log.cpp
  src/ConfigArchive.cpp
  src/File.cpp
  src/Clock.cpp
  src/Path.cpp
  src/Profile.cpp
  src/ProfileDocument.cpp
  src/RuntimeProfile.cpp
  src/Json.cpp
  src/NpcDefinitions.cpp
)
add_library(openkal::platform ALIAS openkal_platform)
add_dependencies(openkal_platform openkal_runtime_icons)
target_compile_definitions(openkal_platform PUBLIC
  OPENKAL_RUNTIME_DATA_ROOT="${OPENKAL_RUNTIME_DATA_ROOT}")

target_include_directories(openkal_platform
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(openkal_platform PUBLIC cxx_std_17)
