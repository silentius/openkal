find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

add_library(openkal_engine_gl
  gl_engine.cpp
  ../TransparentMesh.cpp
)
add_library(openkal::engine ALIAS openkal_engine_gl)

target_include_directories(openkal_engine_gl
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(openkal_engine_gl
  PUBLIC
    OpenGL::GL
    glfw
    GLEW::GLEW
)

target_compile_features(openkal_engine_gl PUBLIC cxx_std_17)
