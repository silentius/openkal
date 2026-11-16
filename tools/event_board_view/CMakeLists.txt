find_package(Freetype REQUIRED)
add_executable(event_board_view
  EventBoardView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/world/ExpEventBoard.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp)
target_include_directories(event_board_view PRIVATE ${CMAKE_SOURCE_DIR}/client/include)
target_link_libraries(event_board_view PRIVATE
  openkal::engine openkal::protocol openkal::platform Freetype::Freetype)
target_compile_features(event_board_view PRIVATE cxx_std_17)
openkal_target_warnings(event_board_view)
