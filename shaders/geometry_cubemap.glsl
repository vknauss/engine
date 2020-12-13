#version 400

layout(triangles) in;
layout(triangle_strip, max_vertices=18) out;

uniform mat4 cubeFaceMatrices[6];

void main() {
    for(int i = 0; i < 6; ++i) {
        for(int j = 0; j < 3; ++j) {
            gl_Layer = i;
            gl_Position = cubeFaceMatrices[i] * gl_in[j].gl_Position;
            EmitVertex();
        }
        EndPrimitive();
    }
}
