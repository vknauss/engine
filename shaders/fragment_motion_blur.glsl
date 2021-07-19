#version 330

in vec2 v_texCoords;

layout(location=0) out vec4 f_color;

uniform sampler2D sceneColor;
uniform sampler2D motionBuffer;
uniform sampler2D depthBuffer;

uniform float blend = 0.9;

uniform int nSteps = 16;
uniform float stepScale = 0.15;

void main() {
    vec2 motion = texture(motionBuffer, v_texCoords).xy * 2.0 - 1.0;
    vec2 motionPX = motion * textureSize(sceneColor, 0).xy;

    float cDepth = texture(depthBuffer, v_texCoords).x;
    float minDepth = cDepth - max(max(abs(dFdx(cDepth)), abs(dFdy(cDepth))), 0.001);


    float mm = max(abs(motionPX.x), abs(motionPX.y));
    float scale = (mm * stepScale > 2.0) ? 2.0 / mm : stepScale;
    vec2 stepSize = scale * motion;

    f_color = texture(sceneColor, v_texCoords);
    float weightSum = 1.0;
    float weight = blend;
    vec2 coordsN = v_texCoords - stepSize;
    vec2 coordsP = v_texCoords + stepSize;
    for (int i = 0; i < nSteps/2; ++i) {
        float weightN = weight * float(texture(depthBuffer, coordsN).x >= minDepth);
        float weightP = weight * float(texture(depthBuffer, coordsP).x >= minDepth);
        f_color += weightN * texture(sceneColor, coordsN);
        f_color += weightP * texture(sceneColor, coordsP);
        coordsN -= stepSize;
        coordsP += stepSize;
        weightSum += weightN + weightP;
        //weight *= blend;
    }
    f_color /= weightSum;
}
