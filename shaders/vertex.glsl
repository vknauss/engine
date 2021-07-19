#version 430

layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoords;
layout(location=5) in vec3 tangent;
layout(location=6) in vec3 bitangent;

uniform uint transformBufferOffset;

struct InstanceTransform {
    mat4 worldViewProj;
    mat4 worldViewMatrixIT;
};

layout(std430, binding = 0) readonly restrict buffer transformData {
    InstanceTransform worldTransforms[];
};

out mat3 tbnViewSpace;
out vec2 texCoords;

out vec4 v_clipPos;

flat out uint uid;

void main() {
    texCoords = vec2(texcoords.x, 1.0-texcoords.y);

    uid = transformBufferOffset + gl_InstanceID;

    mat3 normalWorldView = mat3(worldTransforms[transformBufferOffset + gl_InstanceID].worldViewMatrixIT);
    mat3 tbn = mat3(tangent, bitangent, normal);
    tbnViewSpace = normalWorldView * tbn;

    v_clipPos = worldTransforms[transformBufferOffset + gl_InstanceID].worldViewProj * position;

    gl_Position = v_clipPos;
}
