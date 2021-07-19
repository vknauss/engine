#version 430

layout(location=0) in vec4 position;
layout(location=2) in vec2 texcoords;
layout(location=3) in ivec4 bone_ids;
layout(location=4) in vec4 bone_weights;

struct InstanceTransform {
    mat4 worldViewProj;
    mat4 lastWorldViewProj;
};

layout(std430, binding = 0) readonly restrict buffer transformData {
    InstanceTransform jointTransforms[];
};

uniform uint transformBufferOffset;
uniform uint numJoints;

out vec2 texCoords;

out vec4 v_clipPos;
out vec4 v_lastClipPos;

flat out uint uid;

void main() {
    uid = transformBufferOffset + gl_InstanceID * numJoints;

    texCoords = vec2(texcoords.x, 1.0-texcoords.y);

    mat4 skinningMatrix =
        bone_weights.x * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.x].worldViewProj +
        bone_weights.y * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.y].worldViewProj +
        bone_weights.z * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.z].worldViewProj +
        bone_weights.w * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.w].worldViewProj;

    v_clipPos = skinningMatrix * position;

    mat4 lastSkinningMatrix =
        bone_weights.x * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.x].lastWorldViewProj +
        bone_weights.y * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.y].lastWorldViewProj +
        bone_weights.z * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.z].lastWorldViewProj +
        bone_weights.w * jointTransforms[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.w].lastWorldViewProj;

    v_lastClipPos = lastSkinningMatrix * position;

    gl_Position = v_clipPos;
}
