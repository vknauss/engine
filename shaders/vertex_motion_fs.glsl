#version 330

layout(location=0) in vec3 position;

out vec4 v_clipPos;
out vec4 v_lastClipPos;

uniform mat4 reprojection;
uniform mat4 inverseProjection;

void main() {
    gl_Position = vec4(position, 1.0);

    vec4 positionViewSpace = inverseProjection * vec4(position, 1);
    positionViewSpace = vec4(positionViewSpace.xyz/positionViewSpace.w, 0);

    v_lastClipPos = reprojection * positionViewSpace;
    v_clipPos = vec4(position, 1);
}
