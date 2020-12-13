#version 430

#define MAX_BONES 80

layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoords;
layout(location=3) in ivec4 bone_ids;
layout(location=4) in vec4 bone_weights;
layout(location=5) in vec3 tangent;
layout(location=6) in vec3 bitangent;

uniform mat4 projection;

struct InstanceTransform {
    mat4 worldViewMatrix;
    mat4 worldViewMatrixIT;
};

layout(std430, binding = 0) readonly restrict buffer transformData {
    InstanceTransform jointTransforms[];
};

uniform uint transformBufferOffset;
uniform uint numJoints;

out vec4 positionViewSpace;
out mat3 tbnViewSpace;
out vec2 texCoords;

// these should be the model space transforms!
//uniform mat4 skinningMatrices[MAX_BONES];
//uniform mat3 skinningMatricesIT[MAX_BONES];


void main() {

    texCoords = vec2(texcoords.x, 1.0-texcoords.y);

    mat4 skinningMatrix =
        bone_weights.x * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.x].worldViewMatrix +
        bone_weights.y * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.y].worldViewMatrix +
        bone_weights.z * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.z].worldViewMatrix +
        bone_weights.w * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.w].worldViewMatrix;


    mat4 skinningNormalMatrix =
        bone_weights.x * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.x].worldViewMatrixIT +
        bone_weights.y * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.y].worldViewMatrixIT +
        bone_weights.z * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.z].worldViewMatrixIT +
        bone_weights.w * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.w].worldViewMatrixIT;

    mat3 tbn = mat3(tangent, bitangent, normal);

    //tbnViewSpace = mat3(inverse(transpose(skinningMatrix))) * tbn;
    tbnViewSpace = mat3(skinningNormalMatrix) * tbn;

    positionViewSpace = skinningMatrix * position;
    gl_Position = projection * positionViewSpace;
}
