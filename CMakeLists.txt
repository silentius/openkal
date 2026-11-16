cmake_minimum_required(VERSION 3.20)
project(openkal VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Client/server executables → <build>/bin; tools and tests override this below.
# Libraries (shared + static) → <build>/lib.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
foreach(_config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  string(TOUPPER "${_config}" _config_upper)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_BINARY_DIR}/bin")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_BINARY_DIR}/lib")
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_config_upper} "${CMAKE_BINARY_DIR}/lib")
endforeach()
unset(_config)
unset(_config_upper)

macro(openkal_set_runtime_output dir)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${dir}")
  foreach(_config IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    string(TOUPPER "${_config}" _config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_config_upper} "${dir}")
  endforeach()
endmacro()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(OpenKalWarnings)

option(OPENKAL_BUILD_TESTS "Build unit tests" ON)
option(OPENKAL_BUILD_TOOLS "Build developer tools" ON)
option(OPENKAL_BUILD_AREA_TOOLS
       "Build the area importer and area information tool" ON)
option(OPENKAL_BUILD_AREA_EDITOR
       "Build the interactive area editor (requires OPENKAL_ENGINE=gl)" ON)
option(OPENKAL_BUILD_LOOT_EDITOR "Build Qt loot/droplist editor" OFF)
option(OPENKAL_BUILD_ITEM_VIEWER "Build Qt standalone item viewer" OFF)
option(OPENKAL_BUILD_UI_EDITOR "Build Qt openkal.ui/v1 editor" OFF)
option(OPENKAL_BUILD_MANAGEMENT_CONSOLE "Build Qt server management console" OFF)
option(OPENKAL_BUILD_TEST_PLAYERS "Build Qt multiplayer test population manager" OFF)
option(OPENKAL_BUILD_SETTINGS_APP "Build Qt client settings editor" OFF)
option(OPENKAL_BUILD_PROFILE_CLIENT "Build the gperftools profiling client when available" ON)
option(OPENKAL_RELEASE_BUILD
       "Build installable binaries that load resources from /usr/share/openkal" OFF)

include(OpenKalCoverage)

if(OPENKAL_BUILD_TESTS)
  enable_testing()
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(OPENKAL_DATA_ROOT "" CACHE PATH
    "Optional directory containing user-maintained compatible configuration and runtime data")
option(OPENKAL_INCLUDE_USER_AUDIO
       "Include user-supplied RuntimeData/sounds in a local config.pak" OFF)
set(OPENKAL_EXTERNAL_RESOURCE_PATTERNS)
set(OPENKAL_EXTERNAL_DATA_ARGUMENTS)
if(OPENKAL_DATA_ROOT)
  get_filename_component(OPENKAL_DATA_ROOT "${OPENKAL_DATA_ROOT}" ABSOLUTE)
  if(NOT IS_DIRECTORY "${OPENKAL_DATA_ROOT}")
    message(FATAL_ERROR "OPENKAL_DATA_ROOT does not exist: ${OPENKAL_DATA_ROOT}")
  endif()
  if(NOT IS_DIRECTORY "${OPENKAL_DATA_ROOT}/config" AND
     NOT IS_DIRECTORY "${OPENKAL_DATA_ROOT}/RuntimeData")
    message(FATAL_ERROR
      "OPENKAL_DATA_ROOT must contain config/ or RuntimeData/: ${OPENKAL_DATA_ROOT}")
  endif()
  if(IS_DIRECTORY "${OPENKAL_DATA_ROOT}/config")
    list(APPEND OPENKAL_EXTERNAL_RESOURCE_PATTERNS "${OPENKAL_DATA_ROOT}/config/*")
  endif()
  if(IS_DIRECTORY "${OPENKAL_DATA_ROOT}/RuntimeData")
    list(APPEND OPENKAL_EXTERNAL_RESOURCE_PATTERNS "${OPENKAL_DATA_ROOT}/RuntimeData/*")
  endif()
  list(APPEND OPENKAL_EXTERNAL_DATA_ARGUMENTS --data-root "${OPENKAL_DATA_ROOT}")
endif()
if(OPENKAL_INCLUDE_USER_AUDIO)
  if(NOT OPENKAL_DATA_ROOT)
    message(FATAL_ERROR
      "OPENKAL_INCLUDE_USER_AUDIO requires OPENKAL_DATA_ROOT")
  endif()
  if(OPENKAL_RELEASE_BUILD)
    message(FATAL_ERROR
      "Release packages must not include optional user-supplied audio")
  endif()
  list(APPEND OPENKAL_EXTERNAL_DATA_ARGUMENTS --include-external-audio)
endif()

# Runtime configuration and game data maintained by OpenKal.
if(OPENKAL_RELEASE_BUILD OR CMAKE_BUILD_TYPE STREQUAL "Release")
  # Do not let an existing development-build cache leak its source path into a
  # distributable binary. The Debian package installs config.pak at this path.
  set(OPENKAL_RESOURCE_ROOT "/usr/share/openkal" CACHE PATH
      "Directory containing the OpenKal config directory at runtime" FORCE)
else()
  # Development binaries load the combined archive emitted into the build tree.
  set(OPENKAL_RESOURCE_ROOT "${CMAKE_BINARY_DIR}" CACHE PATH
      "Directory containing OpenKal config.pak at runtime")
endif()
include(OpenKalRuntimeIcons)

file(GLOB_RECURSE OPENKAL_PACKAGED_RESOURCE_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/config/*"
     "${CMAKE_SOURCE_DIR}/RuntimeData/*"
     ${OPENKAL_EXTERNAL_RESOURCE_PATTERNS})
# Track the file set as well as mtimes so removed definitions invalidate the archive.
set(OPENKAL_RESOURCE_MANIFEST "${CMAKE_BINARY_DIR}/packaged-resource-manifest.txt")
file(GENERATE OUTPUT "${OPENKAL_RESOURCE_MANIFEST}"
     CONTENT "${OPENKAL_PACKAGED_RESOURCE_SOURCES}\n")
set(OPENKAL_CONFIG_ARCHIVE "${CMAKE_BINARY_DIR}/config.pak")
add_custom_command(
  OUTPUT "${OPENKAL_CONFIG_ARCHIVE}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/package_config.py"
          "${CMAKE_SOURCE_DIR}" "${OPENKAL_CONFIG_ARCHIVE}"
          ${OPENKAL_EXTERNAL_DATA_ARGUMENTS}
          --generated-runtime-root "${OPENKAL_GENERATED_RUNTIME_ROOT}"
  DEPENDS ${OPENKAL_PACKAGED_RESOURCE_SOURCES} ${OPENKAL_RUNTIME_ICONS}
          "${OPENKAL_ICON_MANIFEST}" "${OPENKAL_RESOURCE_MANIFEST}"
          "${CMAKE_SOURCE_DIR}/tools/package_config.py"
          "${CMAKE_SOURCE_DIR}/tools/convert_runtime_icon.py"
  COMMENT "Packaging OpenKal runtime resource archive"
  VERBATIM)
add_custom_target(openkal_config_archive ALL DEPENDS "${OPENKAL_CONFIG_ARCHIVE}")
add_dependencies(openkal_config_archive openkal_runtime_icons)

set(OPENKAL_ENGINE "gl" CACHE STRING "3D engine backend (null, gl)")
set_property(CACHE OPENKAL_ENGINE PROPERTY STRINGS null gl)

find_package(PNG REQUIRED)

add_subdirectory(libs/platform)
add_subdirectory(libs/settings)
add_subdirectory(libs/map)
add_subdirectory(libs/terrain)
add_subdirectory(libs/protocol)
add_subdirectory(libs/server)
add_subdirectory(engines)
add_subdirectory(client)
add_subdirectory(server)

if(OPENKAL_BUILD_AREA_TOOLS)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/bin/tools")
  add_subdirectory(tools/area_import)
  add_subdirectory(tools/area_info)
endif()

if(OPENKAL_BUILD_AREA_EDITOR)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/bin/tools")
  if(OPENKAL_ENGINE STREQUAL "gl")
    add_subdirectory(tools/area_editor)
  else()
    message(STATUS "  area_editor skipped: OPENKAL_ENGINE=gl is required")
  endif()
endif()

if(OPENKAL_BUILD_TOOLS)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/bin/tools")
  add_subdirectory(tools/skin_audit)
  # Interactive GL tools need glfw/OpenGL; skip them for OPENKAL_ENGINE=null (CI).
  if(OPENKAL_ENGINE STREQUAL "gl")
    add_subdirectory(tools/gtx_view)
    add_subdirectory(tools/character_info_ui_view)
    add_subdirectory(tools/itemgroup_drop_view)
    add_subdirectory(tools/object_view)
    add_subdirectory(tools/event_board_view)
    add_subdirectory(tools/3dItemViewer)
    add_subdirectory(tools/skill_effect_view)
    add_subdirectory(tools/tile_view)
  endif()
endif()

if(OPENKAL_BUILD_LOOT_EDITOR)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/bin/tools")
  add_subdirectory(tools/lootEditor)
endif()

if(OPENKAL_BUILD_ITEM_VIEWER)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/bin/tools")
  add_subdirectory(tools/itemViewer)
endif()

if(OPENKAL_BUILD_UI_EDITOR)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/bin/tools")
  add_subdirectory(tools/uiEditor)
endif()

if(OPENKAL_BUILD_MANAGEMENT_CONSOLE)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/bin/tools")
  add_subdirectory(tools/managementConsole)
endif()

if(OPENKAL_BUILD_TEST_PLAYERS)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/bin/tools")
  add_subdirectory(tools/testPlayers)
endif()

if(OPENKAL_BUILD_SETTINGS_APP)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/bin/tools")
  add_subdirectory(tools/settingsApp)
endif()

if(OPENKAL_BUILD_TESTS)
  openkal_set_runtime_output("${CMAKE_BINARY_DIR}/tests")
  enable_testing()
  add_subdirectory(tests)
endif()

message(STATUS "openkal ${PROJECT_VERSION}")
message(STATUS "  OPENKAL_ENGINE=${OPENKAL_ENGINE}")
message(STATUS "  OPENKAL_RESOURCE_ROOT=${OPENKAL_RESOURCE_ROOT}")
if(OPENKAL_DATA_ROOT)
  message(STATUS "  OPENKAL_DATA_ROOT=${OPENKAL_DATA_ROOT} (optional user data enabled)")
else()
  message(STATUS "  OPENKAL_DATA_ROOT=<disabled> (building project-owned resources only)")
endif()
message(STATUS "  OPENKAL_INCLUDE_USER_AUDIO=${OPENKAL_INCLUDE_USER_AUDIO}")
message(STATUS "  OPENKAL_CONFIG_ARCHIVE=${OPENKAL_CONFIG_ARCHIVE}")

install(FILES "${CMAKE_SOURCE_DIR}/LICENSE"
        DESTINATION "share/doc/openkal"
        RENAME copyright)
install(FILES "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        DESTINATION "share/doc/openkal")
install(FILES "${CMAKE_SOURCE_DIR}/SECURITY.md"
              "${CMAKE_SOURCE_DIR}/docs/ReleaseNotes-0.1.0.md"
              "${CMAKE_SOURCE_DIR}/docs/ReleaseProcedure.md"
        DESTINATION "share/doc/openkal")
install(FILES "${CMAKE_SOURCE_DIR}/third_party/LICENSES/miniaudio-MIT-0.txt"
        DESTINATION "share/doc/openkal/licenses")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

if(UNIX AND NOT APPLE)
  set(CPACK_GENERATOR "DEB")
  set(CPACK_PACKAGE_NAME "openkal-client")
  set(CPACK_PACKAGE_VENDOR "Silentius")
  set(CPACK_PACKAGE_CONTACT "dev@silentius.com")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Open-source KalOnline client")
  string(CONCAT CPACK_DEBIAN_PACKAGE_DESCRIPTION
      "OpenKal is an open-source client for KalOnline-compatible game data.\n"
      "Configure paths to locally supplied compatible client assets before use. "
      "Playable worlds also require matching catalogs and navigation maps. "
      "Additional inputs selected by the builder may be included in config.pak.")
  set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Silentius <dev@silentius.com>")
  set(CPACK_DEBIAN_PACKAGE_SECTION "games")
  set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
  set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "zenity, fonts-dejavu-core")
  set(CPACK_STRIP_FILES "lib/openkal/openkal_client")
  set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
      OWNER_READ OWNER_WRITE OWNER_EXECUTE
      GROUP_READ GROUP_EXECUTE
      WORLD_READ WORLD_EXECUTE)
  # Project source archives use the reviewed manifest, independently of local
  # resource-pack inputs and Git history. Disable CPack's whole-directory export.
  set(CPACK_SOURCE_GENERATOR "")
  include(CPack)
  # Some CMake versions retain package_source and cached source generators.
  # Refuse that generic entry point as well as documenting the safe target.
  file(WRITE "${CMAKE_BINARY_DIR}/CPackSourceConfig.cmake"
       "message(FATAL_ERROR \"Whole-directory source export is disabled. Build target openkal_source_archive instead.\")\n")
endif()

add_custom_target(openkal_source_archive
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/package_source.py"
          --root "${CMAKE_SOURCE_DIR}"
          --version "${PROJECT_VERSION}"
          --output "${CMAKE_BINARY_DIR}/openkal-${PROJECT_VERSION}.tar.gz"
  COMMENT "Packaging manifest-listed OpenKal source files"
  VERBATIM)
