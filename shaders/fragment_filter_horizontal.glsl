#version 330

#define MAX_KERNEL 15

in vec2 v_texCoords;

layout(location=0) out vec4 color;

uniform sampler2D tex_sampler;

uniform int kernelWidth;

uniform float kernelWeights[MAX_KERNEL];
uniform float kernelOffsets[MAX_KERNEL];

uniform vec2 coordOffset;

void main() {
    vec4 col = kernelWeights[0] * texture(tex_sampler, v_texCoords);
    for(int i = 1; i < kernelWidth; i++) {
        col += kernelWeights[i] * texture(tex_sampler, v_texCoords + coordOffset*kernelOffsets[i]);
        col += kernelWeights[i] * texture(tex_sampler, v_texCoords - coordOffset*kernelOffsets[i]);
    }
    color = col;
}
