# engine
C++ rendering engine using OpenGL
Features:
- PBR metallic/roughness materials
- Shadow mapping for point and directional light sources (Using CSM+EVSM)
- HDR rendering + bloom
- Skeletal animation
- Discrete LOD system (LODs must be specified as models, see src/lod_component.h)
- Multithreaded culling and instance list building
- General purpose worker thread job scheduling system
