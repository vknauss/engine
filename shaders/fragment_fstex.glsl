#version 330

in vec2 v_texCoords;

layout(location=0) out vec4 out_color;

uniform sampler2D texSampler;
//uniform samplerCube cubeSampler;

uniform float gamma = 2.2;
uniform float exposure = 5.0;

uniform int enableGammaCorrection = 1;
uniform int enableToneMapping = 1;

uniform int scaleAndShift = 0;

uniform int twoComponent = 0;

uniform vec4 mask = vec4(1);

//uniform int cube = 0;

void main() {
    //vec4 color = (cube == 0) ? texture(texSampler, v_texCoords) : (texture(cubeSampler, vec3(2.0 * v_texCoords - 1.0, 1.0).zxy) * 1.0);
    vec4 color = texture(texSampler, v_texCoords);

    // apply tone mapping
    color.rgb = (enableToneMapping == 1) ? 1.0 - exp(-exposure * color.rgb) : color.rgb;

    // apply gamma correction
    color.rgb = (enableGammaCorrection == 1) ? pow(color.rgb, vec3(1.0/gamma)) : color.rgb;

    //color.rgb = (scaleAndShift == 1) ? 50.0 * (color.rgb - 0.5) + 0.5: color.rgb;
    //color = (twoComponent == 1) ? vec4(color.rg, 0.0, 1.0) : color;

    //out_color = vec4(color.rgb, 1.0);
    out_color = vec4(mask.rgb * color.rgb, mask.a == 1.0 ? color.a : 1.0);
    //out_color = (any(isnan(color)) ? vec4(100, 0, 0, 1) : color);

    //out_color = vec4(0, 1, 1, 1);
}
