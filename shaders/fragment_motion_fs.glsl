#version 330

in vec4 v_clipPos;
in vec4 v_lastClipPos;

layout(location=0) out vec2 f_motion;

void main() {
    f_motion = 0.25 * (v_clipPos.xy - v_lastClipPos.xy/v_lastClipPos.w) + 0.5;
}
