set(CHARACTER_INFO_UI_VIEW_HEADERS
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/BuffSlotBar.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/CharacterInfoWindow.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/FreetypeFont.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/JobTitles.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/NetClient.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/SoftCursor.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/UiDocument.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/UiView.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/UiLayout.h
  ${CMAKE_SOURCE_DIR}/client/include/openkal/client/WidgetTools.h
)

set(CHARACTER_INFO_UI_VIEW_SOURCES
  main.cpp
  ${CMAKE_SOURCE_DIR}/client/src/misc/JobTitles.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/BuffSlotBar.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/CharacterInfoWindow.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/SoftCursor.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiDocument.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/TextWrap.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiLayout.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/WidgetTools.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/UiBmp.cpp
)

add_executable(character_info_ui_view
  ${CHARACTER_INFO_UI_VIEW_SOURCES}
  ${CHARACTER_INFO_UI_VIEW_HEADERS}
)

find_package(Freetype REQUIRED)

target_link_libraries(character_info_ui_view
  PRIVATE
    openkal::engine
    openkal::terrain
    openkal::platform
    openkal::protocol
    openkal::server
    Freetype::Freetype
)

target_include_directories(character_info_ui_view
  PRIVATE
    ${CMAKE_SOURCE_DIR}/client/include
)

target_compile_features(character_info_ui_view PRIVATE cxx_std_17)

target_compile_definitions(character_info_ui_view
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
)
