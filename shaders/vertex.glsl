#version 430

layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoords;
layout(location=5) in vec3 tangent;
layout(location=6) in vec3 bitangent;

uniform mat4 projection;

uniform uint transformBufferOffset;

struct InstanceTransform {
    mat4 worldViewMatrix;
    mat4 worldViewMatrixIT;
};

layout(std430, binding = 0) readonly restrict buffer transformData {
    InstanceTransform worldTransforms[];
};

out vec4 positionViewSpace;
out mat3 tbnViewSpace;
out vec2 texCoords;

void main() {
    texCoords = vec2(texcoords.x, 1.0-texcoords.y);

    mat3 normalWorldView = mat3(worldTransforms[transformBufferOffset + gl_InstanceID].worldViewMatrixIT);
    mat3 tbn = mat3(tangent, bitangent, normal);
    tbnViewSpace = normalWorldView * tbn;

    positionViewSpace = worldTransforms[transformBufferOffset + gl_InstanceID].worldViewMatrix * position;

    gl_Position = projection * positionViewSpace;
}
