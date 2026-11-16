# PNGs are authoring assets. Generate client-ready icons only in the build tree.
execute_process(
  COMMAND "${Python3_EXECUTABLE}" -c "from PIL import Image"
  RESULT_VARIABLE OPENKAL_PILLOW_RESULT
  ERROR_QUIET)
if(NOT OPENKAL_PILLOW_RESULT EQUAL 0)
  message(FATAL_ERROR
    "Runtime icon conversion requires Pillow for ${Python3_EXECUTABLE}. "
    "Install python3-pil or run that interpreter with -m pip install Pillow.")
endif()

set(OPENKAL_GENERATED_RUNTIME_ROOT "${CMAKE_BINARY_DIR}/RuntimeData")
if(OPENKAL_RESOURCE_ROOT STREQUAL CMAKE_SOURCE_DIR)
  set(OPENKAL_RUNTIME_DATA_ROOT "${OPENKAL_GENERATED_RUNTIME_ROOT}")
else()
  set(OPENKAL_RUNTIME_DATA_ROOT "${OPENKAL_RESOURCE_ROOT}/RuntimeData")
endif()

file(GLOB_RECURSE OPENKAL_ICON_TEMPLATES CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/RuntimeData/templates/*.[pP][nN][gG]")
# Authoring masters stay beside their exports but are never runtime icons.
list(FILTER OPENKAL_ICON_TEMPLATES EXCLUDE REGEX "/templates/(.*/)?source/")
# HUD textures retain native dimensions and RGBA instead of becoming 32px icons.
list(FILTER OPENKAL_ICON_TEMPLATES EXCLUDE REGEX "/templates/(dragon_hud|dragon_minimap|dragon_menu|dragon_dialog)/")
set(OPENKAL_RUNTIME_ICONS)
foreach(template IN LISTS OPENKAL_ICON_TEMPLATES)
  file(RELATIVE_PATH relative "${CMAKE_SOURCE_DIR}/RuntimeData/templates" "${template}")
  string(REGEX REPLACE "\\.[pP][nN][gG]$" ".bmp" relative "${relative}")
  set(output "${OPENKAL_GENERATED_RUNTIME_ROOT}/HyperText/${relative}")
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/convert_runtime_icon.py"
            "${template}" "${output}"
    DEPENDS "${template}" "${CMAKE_SOURCE_DIR}/tools/convert_runtime_icon.py"
    COMMENT "Converting runtime icon ${relative}"
    VERBATIM)
  list(APPEND OPENKAL_RUNTIME_ICONS "${output}")
  get_filename_component(relative_dir "${relative}" DIRECTORY)
  # Loose counterparts also support profiles that deliberately unmount config.pak.
  install(FILES "${output}" DESTINATION "share/openkal/RuntimeData/HyperText/${relative_dir}")
endforeach()
file(GLOB OPENKAL_HUD_TEXTURES CONFIGURE_DEPENDS
     "${CMAKE_SOURCE_DIR}/RuntimeData/templates/dragon_hud/bmp/*.bmp"
     "${CMAKE_SOURCE_DIR}/RuntimeData/templates/dragon_menu/bmp/*.bmp"
     "${CMAKE_SOURCE_DIR}/RuntimeData/templates/dragon_dialog/bmp/*.bmp"
     "${CMAKE_SOURCE_DIR}/RuntimeData/templates/dragon_minimap/bmp/*.bmp")
foreach(texture IN LISTS OPENKAL_HUD_TEXTURES)
  get_filename_component(name "${texture}" NAME)
  get_filename_component(bmp_dir "${texture}" DIRECTORY)
  get_filename_component(hud_dir "${bmp_dir}" DIRECTORY)
  get_filename_component(hud_name "${hud_dir}" NAME)
  set(output "${OPENKAL_GENERATED_RUNTIME_ROOT}/UI/${hud_name}/${name}")
  add_custom_command(OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${OPENKAL_GENERATED_RUNTIME_ROOT}/UI/${hud_name}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${texture}" "${output}"
    DEPENDS "${texture}" VERBATIM)
  list(APPEND OPENKAL_RUNTIME_ICONS "${output}")
  install(FILES "${output}" DESTINATION "share/openkal/RuntimeData/UI/${hud_name}")
endforeach()
add_custom_target(openkal_runtime_icons ALL DEPENDS ${OPENKAL_RUNTIME_ICONS})

# A removed template must also invalidate the archive, even if its old BMP remains.
set(OPENKAL_ICON_MANIFEST "${CMAKE_BINARY_DIR}/runtime-icon-manifest.txt")
file(GENERATE OUTPUT "${OPENKAL_ICON_MANIFEST}" CONTENT "${OPENKAL_ICON_TEMPLATES};${OPENKAL_HUD_TEXTURES}\n")
