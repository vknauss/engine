#version 330

in vec2 v_texCoords;

layout(location=0) out vec4 f_color;

uniform sampler2D currentFrame;
uniform sampler2D history;
uniform sampler2D motionBuffer;

uniform uint frame;

// not sure what to call it but lets say temporal ordered dithering? based on
// https://www.shadertoy.com/view/ttcSD8
// I think this originally comes from the guerrilla horizon:zero dawn presentation
const int bayer4x4[16] = int[16] ( 0,  8,  2, 10,
                                  12,  4, 14,  6,
                                   3, 11,  1,  9,
                                  15,  7, 13,  5);

void main() {
    ivec2 pixelCoords = ivec2(gl_FragCoord.xy);
    ivec2 bayerCoords = ivec2(pixelCoords.x%4, pixelCoords.y%4);
    vec2 motion = 2.0 * texture(motionBuffer, v_texCoords).xy - 1.0;
    vec2 prevCoords = v_texCoords - motion;
    if (bayer4x4[bayerCoords.y*4+bayerCoords.x] == int(frame%16u)) {
        f_color = texelFetch(currentFrame, pixelCoords/4, 0);
    } else if (any(greaterThanEqual(prevCoords, vec2(1.0))) || any(lessThanEqual(prevCoords, vec2(0.0)))) {
        // use linear filtering to reduce artifacts
        f_color = texture(currentFrame, v_texCoords);
    } else {
        f_color = texture(history, v_texCoords - motion);
    }
}
