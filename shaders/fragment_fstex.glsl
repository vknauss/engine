#version 330

in vec2 v_texCoords;

layout(location=0) out vec4 out_color;

uniform sampler2D texSampler;
uniform samplerCube cubeSampler;

uniform float gamma = 2.2;
uniform float exposure = 5.0;

uniform int enableGammaCorrection = 1;
uniform int enableToneMapping = 1;

uniform int cube = 0;

void main() {
    vec4 color = (cube == 0) ? texture(texSampler, v_texCoords) : (texture(cubeSampler, vec3(2.0 * v_texCoords - 1.0, 1.0).zxy) * 1.0);
    //vec4 color = texture(texSampler, v_texCoords);

    // apply tone mapping
    color.rgb = (enableToneMapping == 1) ? 1.0 - exp(-exposure * color.rgb) : color.rgb;

    // apply gamma correction
    color.rgb = (enableGammaCorrection == 1) ? pow(color.rgb, vec3(1.0/gamma)) : color.rgb;

    out_color = vec4(color.rgb, 1.0);
    //out_color = color;
    //out_color = (any(isnan(color)) ? vec4(100, 0, 0, 1) : color);
}
