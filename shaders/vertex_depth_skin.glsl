#version 430

#define MAX_BONES 80

layout(location=0) in vec4 position;
layout(location=3) in ivec4 bone_ids;
layout(location=4) in vec4 bone_weights;


layout(std430, binding = 0) readonly restrict buffer transformData {
    mat4 skinningMatrices[];
};

uniform uint numJoints;
uniform uint transformBufferOffset;

void main() {

    mat4 skinningMatrix =
        bone_weights.x * skinningMatrices[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.x] +
        bone_weights.y * skinningMatrices[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.y] +
        bone_weights.z * skinningMatrices[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.z] +
        bone_weights.w * skinningMatrices[transformBufferOffset + gl_InstanceID * numJoints + bone_ids.w];

    gl_Position = skinningMatrix * position;
}
