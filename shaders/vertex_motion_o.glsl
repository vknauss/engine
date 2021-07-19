#version 430

layout(location=0) in vec4 position;
layout(location=2) in vec2 texcoords;

uniform uint transformBufferOffset;

struct InstanceTransform {
    mat4 worldViewProj;
    mat4 lastWorldViewProj;
};

layout(std430, binding = 0) readonly restrict buffer transformData {
    InstanceTransform worldTransforms[];
};

out vec2 texCoords;

out vec4 v_clipPos;
out vec4 v_lastClipPos;

flat out uint uid;

void main() {
    texCoords = vec2(texcoords.x, 1.0-texcoords.y);

    uid = transformBufferOffset + gl_InstanceID;

    v_clipPos = worldTransforms[transformBufferOffset + gl_InstanceID].worldViewProj * position;

    v_lastClipPos = worldTransforms[transformBufferOffset + gl_InstanceID].lastWorldViewProj * position;

    gl_Position = v_clipPos;
}

