#version 430

layout(location=0) in vec4 position;

layout(std430, binding = 0) readonly restrict buffer transformData {
    mat4 MVP[];
};

uniform uint transformBufferOffset;

void main() {
    gl_Position = MVP[transformBufferOffset + gl_InstanceID] * position;
}
