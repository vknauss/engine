// When loading shader source, include a block formatted like the following:
//
// #version 330
// const int WIDTH = 3;
// const float WEIGHTS[3] = float[3](0.20236, 0.303053, 0.095766);
// const float OFFSETS[3] = float[3](0.0, 1.4092, 3.2979);
//
// note OFFSETS[0] should always be 0.0

in vec2 v_texCoords;

layout(location=0) out vec4 color;

uniform sampler2D tex_sampler;

uniform vec2 coordOffset;

void main() {
    vec4 col = WEIGHTS[0] * texture(tex_sampler, v_texCoords);
    for (int i = 1; i < WIDTH; ++i) {
        col += WEIGHTS[i] * texture(tex_sampler, v_texCoords + coordOffset * OFFSETS[i]);
        col += WEIGHTS[i] * texture(tex_sampler, v_texCoords - coordOffset * OFFSETS[i]);
    }
    color = col;
}
