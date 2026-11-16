add_executable(skin_asset_probe SkinAssetProbe.cpp SkinAssetInspection.cpp)
target_link_libraries(skin_asset_probe PRIVATE openkal::terrain openkal::platform)
openkal_target_warnings(skin_asset_probe)

if(OPENKAL_ENGINE STREQUAL "gl")
  add_executable(skin_preview_renderer SkinPreviewRenderer.cpp SkinPreview.cpp)
  target_link_libraries(skin_preview_renderer PRIVATE
    openkal::engine openkal::terrain openkal::platform PNG::PNG)
  openkal_target_warnings(skin_preview_renderer)
endif()
