#version 330

in vec2 v_texCoords;

uniform sampler2D texSampler;

/*const float blurFilter[25] = float[25](1.0/273.0,  4.0/273.0,  7.0/273.0,  4.0/273.0, 1.0/273.0,
                                       4.0/273.0, 16.0/273.0, 26.0/273.0, 16.0/273.0, 4.0/273.0,
                                       7.0/273.0, 26.0/273.0, 41.0/273.0, 26.0/273.0, 7.0/273.0,
                                       4.0/273.0, 16.0/273.0, 26.0/273.0, 16.0/273.0, 4.0/273.0,
                                       1.0/273.0,  4.0/273.0,  7.0/273.0,  4.0/273.0, 1.0/273.0);*/
const float blurFilter[9] = float[9](1.0/16.0, 1.0/8.0, 1.0/16.0,
                                     1.0/8.0,  1.0/4.0, 1.0/8.0,
                                     1.0/16.0, 1.0/8.0, 1.0/16.0);
const float sharpFilter[9] = float[9](-1.0/9.0, -1.0/9.0, -1.0/9.0,
                                      -1.0/9.0,  1.0,     -1.0/9.0,
                                      -1.0/9.0, -1.0/9.0, -1.0/9.0);
const int filterWidth = 3;

uniform float amount = 0.1;

layout(location = 0) out vec4 f_color;

void main() {
    vec2 pixelSize = 1.0 / textureSize(texSampler, 0).xy;

    vec4 middle = vec4(1.0);
    vec4 result = vec4(0.0);
    int ofs = filterWidth/2;
    for (int i = 0; i < filterWidth; ++i) for (int j = 0; j < filterWidth; ++j) {
        vec4 s = texture(texSampler, v_texCoords + pixelSize * vec2(i-ofs, j-ofs));
        s /= s.a;
        if (i == ofs && j == ofs) middle = s;
        result += blurFilter[i*filterWidth+j] * s;
    }
    result /= result.a;
    //f_color = result;
    f_color = mix(middle, result, -amount);
}
