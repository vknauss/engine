#version 330

in vec2 v_texCoords;

uniform sampler2DArray depthTextureArray;
uniform int arrayLayer;
uniform int enableEVSM;

const float cPos = 42.0;
const float cNeg = 14.0;

layout(location=0) out vec4 op;


void main() {
    float depth = texture(depthTextureArray, vec3(v_texCoords, arrayLayer)).x;

    if(enableEVSM == 1) {
        op = vec4(exp(cPos * depth), 0, -exp(-cNeg * depth), 0);
        op.yw = op.xz * op.xz;
    } else {
        op = vec4(depth, depth*depth, 0, 1);
    }
    //op = vec4(1, 1, 0, 1);
}
