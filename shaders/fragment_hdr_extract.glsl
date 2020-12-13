#version 330

in vec2 v_texCoords;

layout(location=0) out vec4 out_color;

uniform sampler2D texSampler;

const float threshold = 1.0;
const vec3 lum = vec3(0.2126, 0.7152, 0.0722);

void main() {
    vec4 color = texture(texSampler, v_texCoords);

    float l = dot(color.rgb, lum);
    float c = max(0.0, (l - threshold) / max(l, 0.001));

    out_color = vec4(c * color.rgb, 1.0);
}
