#version 330

in vec2 texCoords;

in vec4 v_clipPos;
in vec4 v_lastClipPos;

flat in uint uid;

layout(location=0) out vec2 f_motion;

float random(vec2 coord) {
    return fract(sin(dot(coord, vec2(12.9898,78.233))) * 43758.5453123);
}

void main() {
    vec2 clipPos     = 0.5 + 0.5 * (v_clipPos.xy / v_clipPos.w);
    vec2 lastClipPos = 0.5 + 0.5 * (v_lastClipPos.xy / v_lastClipPos.w);
    f_motion = 0.5 + 0.5 * (clipPos - lastClipPos);
}
