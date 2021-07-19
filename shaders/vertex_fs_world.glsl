#version 410 core

layout(location=0) in vec3 position;
layout(location=1) in vec2 texCoords;

out vec3 v_worldDir;
out vec2 v_texCoords;

uniform mat4 inverseView;
uniform mat4 inverseProjection;

uniform vec2 pixelOffset;

uniform float tanFovY;
uniform float tanFovX;

void main() {
    gl_Position = vec4(position, 1.0);

    v_texCoords = texCoords;

    /*vec4 positionViewSpace = inverseProjection * vec4(position, 1.0);
    positionViewSpace = vec4(positionViewSpace.xyz/positionViewSpace.w, 0.0);
    //positionViewSpace = vec4(positionViewSpace.xyz, 0.0);
    v_worldDir = normalize(vec3(inverseView * positionViewSpace));*/

    vec4 viewDir = vec4((position.xy + pixelOffset) * vec2(tanFovX, tanFovY), -1.0, 0.0);
    v_worldDir = normalize(vec3(inverseView * viewDir));

    //v_worldDir = normalize(vec3(inverseViewProj * vec4(position, 1.0)));
}
