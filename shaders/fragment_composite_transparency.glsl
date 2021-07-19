#version 330

in vec2 v_texCoords;

layout(location=0) out vec4 f_color;

uniform sampler2D accumTexture;
uniform sampler2D revealageTexture;

void main() {
    float revealage = texture(revealageTexture, v_texCoords).r;

    if (revealage == 1.0) discard;

    vec4 accum = texture(accumTexture, v_texCoords);
    if (any(isinf(abs(accum.rgb)))) accum.rgb = accum.aaa;

    vec3 averageColor = accum.rgb / max(accum.a, 0.0001);

    f_color = vec4(averageColor, 1.0 - revealage);
}
