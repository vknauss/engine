#version 330

in vec2 v_texCoords;

layout(location=0) out vec2 f_motion;

uniform sampler2D gBufferDepth;

uniform highp mat4 lastViewProj;
uniform highp mat4 inverseViewProj;

//uniform vec2 jitter;

void main() {
    //highp vec4 cvvPos = vec4(2.0 * vec3(v_texCoords + 0.5 * jitter, texture(gBufferDepth, v_texCoords + 0.5 * jitter).x) - 1.0, 1.0);
    highp vec4 cvvPos = vec4(2.0 * vec3(v_texCoords, texture(gBufferDepth, v_texCoords).x) - 1.0, 1.0);
    highp vec4 worldPos = inverseViewProj * cvvPos;
    worldPos /= worldPos.w;
    highp vec4 prevCoords = lastViewProj * worldPos;
    prevCoords = 0.5 + 0.5 * prevCoords / prevCoords.w;

    //f_motion = vec4((v_texCoords - prevCoords.xy) * 0.5 + 0.5, 0.0, 1.0);
    f_motion = (v_texCoords - prevCoords.xy) * 0.5 + 0.5;
    //f_motion = vec2(0.0, 1.0);
}
