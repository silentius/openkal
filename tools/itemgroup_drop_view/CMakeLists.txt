set(ITEMGROUP_DROP_VIEW_HEADERS
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/Equipment.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/FreetypeFont.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/Inventory.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/ItemDb.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/ItemTooltip.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/PrefixDb.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/ShinsuDb.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/SoftCursor.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/UiDocument.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/UiLayout.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/UiView.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/WidgetTools.h
)

set(ITEMGROUP_DROP_VIEW_SOURCES
  main.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Equipment.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/Inventory.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/ItemTooltip.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/MixVisual.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ItemDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/PrefixDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/ShinsuDb.cpp
  ${CMAKE_SOURCE_DIR}/client/src/player/Player.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/SoftCursor.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
)

add_executable(itemgroup_drop_view
  ${ITEMGROUP_DROP_VIEW_SOURCES}
  ${ITEMGROUP_DROP_VIEW_HEADERS}
)

find_package(Freetype REQUIRED)

target_link_libraries(itemgroup_drop_view
  PRIVATE
    openkal::engine
    openkal::terrain
    openkal::platform
    openkal::server
    Freetype::Freetype
)

target_include_directories(itemgroup_drop_view
  PRIVATE
    ${CMAKE_SOURCE_DIR}/client/include
)

target_compile_features(itemgroup_drop_view PRIVATE cxx_std_17)

target_compile_definitions(itemgroup_drop_view
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
)
