#version 430

#define MAX_BONES 80

layout(location=0) in vec4 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoords;
layout(location=3) in ivec4 bone_ids;
layout(location=4) in vec4 bone_weights;
layout(location=5) in vec3 tangent;
layout(location=6) in vec3 bitangent;

struct InstanceTransform {
    mat4 worldViewProj;
    mat4 worldViewIT;
};

layout(std430, binding = 0) readonly restrict buffer transformData {
    InstanceTransform jointTransforms[];
};

uniform uint transformBufferOffset;
uniform uint numJoints;

out vec4 positionViewSpace;
out mat3 tbnViewSpace;
out vec2 texCoords;

out vec4 v_clipPos;

flat out uint uid;

// these should be the model space transforms!
//uniform mat4 skinningMatrices[MAX_BONES];
//uniform mat3 skinningMatricesIT[MAX_BONES];


void main() {

    uid = transformBufferOffset + gl_InstanceID * numJoints;

    texCoords = vec2(texcoords.x, 1.0-texcoords.y);

    mat4 skinningMatrix =
        bone_weights.x * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.x].worldViewProj +
        bone_weights.y * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.y].worldViewProj +
        bone_weights.z * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.z].worldViewProj +
        bone_weights.w * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.w].worldViewProj;


    mat4 skinningNormalMatrix =
        bone_weights.x * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.x].worldViewIT +
        bone_weights.y * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.y].worldViewIT +
        bone_weights.z * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.z].worldViewIT +
        bone_weights.w * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.w].worldViewIT;

    mat3 tbn = mat3(tangent, bitangent, normal);

    //tbnViewSpace = mat3(inverse(transpose(skinningMatrix))) * tbn;
    tbnViewSpace = mat3(skinningNormalMatrix) * tbn;

    v_clipPos = skinningMatrix * position;

    gl_Position = v_clipPos;

}
