cmake_minimum_required(VERSION 3.12)

project(Several)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -fsanitize=address")

set(CMAKE_SOURCE_DIR ${CMAKE_HOME_DIRECTORY}/src)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SRC ${CMAKE_SOURCE_DIR})
set(SOURCES 
    ${SRC}/main.cc
    ${SRC}/core/job_scheduler.cc
    ${SRC}/core/animation/animation_blend_tree.cc
    ${SRC}/core/animation/animation_instance.cc
    ${SRC}/core/animation/animation_state_graph.cc
    ${SRC}/core/animation/animation_system.cc
    ${SRC}/core/animation/animation.cc
    ${SRC}/core/animation/joint.cc
    ${SRC}/core/animation/skeleton.cc
    ${SRC}/core/app/app.cc
    ${SRC}/core/app/input_context.cc
    ${SRC}/core/app/input_manager.cc
    ${SRC}/core/app/window.cc
    ${SRC}/core/audio/audio.cc
    ${SRC}/core/ecs/game_world.cc
    ${SRC}/core/physics/physics.cc
    ${SRC}/core/render/frustum_culler.cc
    ${SRC}/core/render/fullscreen_quad.cc
    ${SRC}/core/render/instance_list_builder.cc
    ${SRC}/core/render/render_buffer.cc
    ${SRC}/core/render/render_debug.cc
    ${SRC}/core/render/render_layer.cc
    ${SRC}/core/render/renderer.cc
    ${SRC}/core/render/shader.cc
    ${SRC}/core/render/passes/background_motion_vectors_pass.cc
    ${SRC}/core/render/passes/bloom_pass.cc
    ${SRC}/core/render/passes/deferred_directional_light_pass.cc
    ${SRC}/core/render/passes/deferred_pass.cc
    ${SRC}/core/render/passes/deferred_point_light_pass.cc
    ${SRC}/core/render/passes/gbuffer_pass.cc
    ${SRC}/core/render/passes/geometry_render_pass.cc
    ${SRC}/core/render/passes/motion_blur_pass.cc
    ${SRC}/core/render/passes/motion_vectors_pass.cc
    ${SRC}/core/render/passes/object_motion_vectors_pass.cc
    ${SRC}/core/render/passes/point_shadow_face_pass.cc
    ${SRC}/core/render/passes/point_shadow_pass.cc
    ${SRC}/core/render/passes/shadow_cascade_pass.cc
    ${SRC}/core/render/passes/shadow_map_pass.cc
    ${SRC}/core/render/passes/ssao_pass.cc
    ${SRC}/core/render/passes/taa_pass.cc
    ${SRC}/core/render/passes/transparency_composite_pass.cc
    ${SRC}/core/render/passes/transparency_pass.cc
    ${SRC}/core/render/passes/volumetric_clouds_pass.cc
    ${SRC}/core/resources/lod_component.cc
    ${SRC}/core/resources/material.cc
    ${SRC}/core/resources/mesh_data.cc
    ${SRC}/core/resources/mesh.cc
    ${SRC}/core/resources/model.cc
    ${SRC}/core/resources/resource_load.cc
    ${SRC}/core/resources/skeleton_description.cc
    ${SRC}/core/resources/texture.cc
    ${SRC}/core/scene/camera.cc
    ${SRC}/core/scene/directional_light.cc
    ${SRC}/core/scene/point_light.cc
    ${SRC}/core/scene/renderable.cc
    ${SRC}/core/scene/scene.cc
    ${SRC}/core/util/math_util.cc
    ${SRC}/core/util/mesh_builder.cc
    ${SRC}/core/util/timer.cc
    ${SRC}/editor/imvec_operators.cc
    ${SRC}/editor/editor_gui.cc
    ${SRC}/editor/transform_gizmos.cc
    ${SRC}/editor/imgui/imgui_demo.cpp
    ${SRC}/editor/imgui/imgui_draw.cpp
    ${SRC}/editor/imgui/imgui_impl_glfw.cpp
    ${SRC}/editor/imgui/imgui_impl_opengl3.cpp
    ${SRC}/editor/imgui/imgui_tables.cpp
    ${SRC}/editor/imgui/imgui_widgets.cpp
    ${SRC}/editor/imgui/imgui.cpp)

find_package(glfw3 3.3 REQUIRED)

find_package(GLEW REQUIRED)

set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-3.0)

set(LIBS_DIR "/home/vincent/code/lib")

add_subdirectory(${LIBS_DIR}/assimp ${CMAKE_BINARY_DIR}/extern/assimp EXCLUDE_FROM_ALL)
add_subdirectory(${LIBS_DIR}/bullet3-2.88 ${CMAKE_BINARY_DIR}/extern/bullet EXCLUDE_FROM_ALL)
add_subdirectory(${LIBS_DIR}/entt ${CMAKE_BINARY_DIR}/extern/entt EXCLUDE_FROM_ALL)
add_subdirectory(${LIBS_DIR}/glm ${CMAKE_BINARY_DIR}/extern/glm EXCLUDE_FROM_ALL)

add_compile_definitions(
    GLEW_STATIC
    GLM_ENABLE_EXPERIMENTAL
    IMGUI_IMPL_OPENGL_LOADER_GLEW)

add_executable(Several ${SOURCES})

target_include_directories(Several PUBLIC
    ${SRC}
    ${LIBS_DIR}
    ${LIBS_DIR}/bullet3-2.88/src)

target_link_libraries(Several
    glfw 
    GLEW::GLEW 
    OpenGL::GL 
    glm::glm 
    EnTT::EnTT 
    BulletDynamics BulletCollision LinearMath 
    assimp 
    stdc++fs
    portaudio
    PkgConfig::SNDFILE 
    PkgConfig::GTKMM
    debug -fsanitize=address
)
