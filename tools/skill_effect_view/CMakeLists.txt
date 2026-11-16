set(SKILL_EFFECT_VIEW_SOURCES
  main.cpp
  EffectCatalog.cpp
  SkillEffectView.cpp
  ${CMAKE_SOURCE_DIR}/client/src/ui/FreetypeFont.cpp
)

add_executable(skill_effect_view
  ${SKILL_EFFECT_VIEW_SOURCES}
  ${CMAKE_SOURCE_DIR}/client/src/world/SpriteFx.cpp
)

find_package(Freetype REQUIRED)

target_link_libraries(skill_effect_view
  PRIVATE
    openkal::engine
    openkal::terrain
    openkal::platform
    Freetype::Freetype
)

target_include_directories(skill_effect_view
  PRIVATE
    ${CMAKE_SOURCE_DIR}/client/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(skill_effect_view PRIVATE cxx_std_17)

target_compile_definitions(skill_effect_view
  PRIVATE
    OPENKAL_REPO_ROOT="${OPENKAL_RESOURCE_ROOT}"
)
