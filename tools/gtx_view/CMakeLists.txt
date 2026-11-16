add_executable(gtx_view main.cpp)

target_link_libraries(gtx_view
  PRIVATE
    openkal::engine
    openkal::terrain
    openkal::platform
)

target_compile_features(gtx_view PRIVATE cxx_std_17)
target_compile_definitions(gtx_view PRIVATE OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}")
