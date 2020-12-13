#version 330

layout(location = 0) in vec4 position;

uniform mat4 modelViewProj;

void main() {
    gl_Position = modelViewProj * position;
}
