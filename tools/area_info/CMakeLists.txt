add_executable(area_info main.cpp)
target_link_libraries(area_info PRIVATE openkal::map openkal::platform)
target_compile_definitions(area_info PRIVATE OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}")
