#version 330

#define MAX_KERNEL 15

in vec2 v_texCoords;

layout(location=0) out vec4 color;

uniform sampler2D tex_sampler;

uniform int kernel_w;
uniform int kernel_h;

uniform float kernel[MAX_KERNEL*MAX_KERNEL];


void main() {
    vec4 col = vec4(0.0);
    vec2 d = vec2(1.0) / textureSize(tex_sampler, 0).xy;
    for(int i = 0; i < kernel_w; i++) {
        for(int j = 0; j < kernel_h; j++) {
            col += kernel[i*kernel_h+j] * texture(tex_sampler, v_texCoords+d*vec2(i-(kernel_w/2), j-(kernel_h/2)));
        }
    }
    color = col;
}
