#version 330

in vec2 v_texCoords;

layout(location=0) out vec4 f_color;

uniform sampler2D motionBlurTexture;
uniform sampler2D taaTexture;
uniform sampler2D motionBuffer;

uniform vec2 jitter;

void main() {
    vec2 motion = texture(motionBuffer, v_texCoords + 0.5 * jitter).xy * 2.0 - 1.0;

    float motionLengthPX = length(motion * textureSize(taaTexture, 0).xy);

    float blend = clamp((motionLengthPX - 2.0) / 14.0, 0.0, 1.0);

    f_color = mix(texture(taaTexture, v_texCoords), texture(motionBlurTexture, v_texCoords + 0.5 * jitter), blend);
}
