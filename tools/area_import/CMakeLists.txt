add_executable(area_import main.cpp LegacyAreaImporter.cpp)
target_link_libraries(area_import PRIVATE openkal::map openkal::platform)
target_compile_features(area_import PRIVATE cxx_std_17)
target_compile_definitions(area_import PRIVATE OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}")
