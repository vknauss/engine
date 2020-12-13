# vknauss/engine

C++ rendering engine using OpenGL

## Features:
- PBR metallic/roughness materials
- Shadow mapping for point and directional light sources (Using CSM+EVSM)
- HDR rendering + bloom
- Skeletal animation
- Discrete LOD system (LODs must be specified as models, see src/lod_component.h)
- Multithreaded culling and instance list building
- General purpose worker thread job scheduling system

## Building:

Build system is not configured at the moment. All the source to be compiled is in src/. The project must be compiled+linked against GLEW, GLFW, and assimp. Additionally, the compiler will need to be pointed to include the stbi_image library headers. Good luck friends. Makefile coming soon?
