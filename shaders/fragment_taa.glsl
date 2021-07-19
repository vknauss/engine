#version 330

in vec2 v_texCoords;

layout(location = 0) out vec4 f_result;
//layout(location = 1) out vec4 f_motion;

uniform sampler2D currentBuffer;
uniform sampler2D historyBuffer;
uniform sampler2D motionBuffer;

uniform sampler2D gBufferDepth;

uniform highp mat4 lastViewProj;
uniform highp mat4 inverseViewProj;

uniform float persistence = 0.9;

uniform vec2 jitter;

// from https://en.wikipedia.org/wiki/YCoCg
const mat3 rgbToYCC = mat3(0.25, 0.5, -0.25, 0.5, 0.0, 0.5, 0.25, -0.5, -0.25);
const mat3 yccToRGB = mat3(1, 1, 1, 1, 0, -1, -1, 1, -1);


// based on
// Temporal Reprojection Anti-Aliasing in INSIDE by Lasse Jon Fuglsang Pedersen
// https://ubm-twvideo01.s3.amazonaws.com/o1/vault/gdc2016/Presentations/Pedersen_LasseJonFuglsang_TemporalReprojectionAntiAliasing.pdf
vec3 clip(vec3 bMin, vec3 bMax, vec3 value) {
    vec3 c = 0.5 * (bMax + bMin);
    vec3 e = 0.5 * (bMax - bMin);
    vec3 v = value - c;
    vec3 a = abs(v / e);
    float m = max(a.x, max(a.y, a.z));
    if (m > 1.0)
        return c + v / m;
    return value;
}

void neighborhoodMinMax(vec2 uv, vec2 pixelSize, out vec3 nmin, out vec3 nmax) {
    vec3 sw = rgbToYCC * texture(currentBuffer, uv - pixelSize).rgb;
    vec3 s  = rgbToYCC * texture(currentBuffer, uv - vec2(0.0, pixelSize.y)).rgb;
    vec3 se = rgbToYCC * texture(currentBuffer, uv + vec2(pixelSize.x, -pixelSize.y)).rgb;
    vec3 w  = rgbToYCC * texture(currentBuffer, uv - vec2(pixelSize.x, 0.0)).rgb;
    vec3 m  = rgbToYCC * texture(currentBuffer, uv).rgb;
    vec3 e  = rgbToYCC * texture(currentBuffer, uv + vec2(pixelSize.x, 0.0)).rgb;
    vec3 nw = rgbToYCC * texture(currentBuffer, uv + vec2(-pixelSize.x, pixelSize.y)).rgb;
    vec3 n  = rgbToYCC * texture(currentBuffer, uv + vec2(0.0, pixelSize.y)).rgb;
    vec3 ne = rgbToYCC * texture(currentBuffer, uv + pixelSize).rgb;

    vec3 minPlus = min(min(min(min(s, w), m), e), n);
    vec3 min3x3  = min(min(min(min(sw, se), nw), ne), minPlus);

    vec3 maxPlus = max(max(max(max(s, w), m), e), n);
    vec3 max3x3  = max(max(max(max(sw, se), nw), ne), maxPlus);

    nmin = 0.5 * minPlus + 0.5 * min3x3;
    nmax = 0.5 * maxPlus + 0.5 * max3x3;
}

void neighborhoodMinMaxRGB(vec2 uv, vec2 pixelSize, out vec3 nmin, out vec3 nmax) {
    vec3 sw = texture(currentBuffer, uv - pixelSize).rgb;
    vec3 s  = texture(currentBuffer, uv - vec2(0.0, pixelSize.y)).rgb;
    vec3 se = texture(currentBuffer, uv + vec2(pixelSize.x, -pixelSize.y)).rgb;
    vec3 w  = texture(currentBuffer, uv - vec2(pixelSize.x, 0.0)).rgb;
    vec3 m  = texture(currentBuffer, uv).rgb;
    vec3 e  = texture(currentBuffer, uv + vec2(pixelSize.x, 0.0)).rgb;
    vec3 nw = texture(currentBuffer, uv + vec2(-pixelSize.x, pixelSize.y)).rgb;
    vec3 n  = texture(currentBuffer, uv + vec2(0.0, pixelSize.y)).rgb;
    vec3 ne = texture(currentBuffer, uv + pixelSize).rgb;

    vec3 minPlus = min(min(min(min(s, w), m), e), n);
    vec3 min3x3  = min(min(min(min(sw, se), nw), ne), minPlus);

    vec3 maxPlus = max(max(max(max(s, w), m), e), n);
    vec3 max3x3  = max(max(max(max(sw, se), nw), ne), maxPlus);

    nmin = 0.5 * minPlus + 0.5 * min3x3;
    nmax = 0.5 * maxPlus + 0.5 * max3x3;
}

vec2 sampleMotion(vec2 uv, vec2 pixelSize) {
    int mini = 0;
    float mind = texture(gBufferDepth, uv - pixelSize).x;
    for (int i = 1; i < 9; ++i) {
        int su = i/3 - 1;
        int sv = i%3 - 1;
        float d = texture(gBufferDepth, uv + pixelSize * vec2(su, sv)).x;
        if (d < mind) {
            mini = i;
            mind = d;
        }
    }

    int su = mini/3 - 1;
    int sv = mini%3 - 1;
    return 2.0 * texture(motionBuffer, uv + pixelSize * vec2(su, sv)).xy - 1.0;
    //return 2.0 * texture(motionBuffer, uv).xy - 1.0;
}

void main() {
    vec2 resolution = textureSize(currentBuffer, 0).xy;

    vec2 motion = texture(motionBuffer, v_texCoords + 0.5 * jitter).xy * 2.0 - 1.0;
    //vec2 motion = sampleMotion(v_texCoords + 0.5 * jitter, 1.0 / resolution);
    vec2 prevCoords;

    // bias the motion toward pixel centers
    vec2 motionPX = motion * resolution;
    motionPX = (floor(motionPX) + smoothstep(0.0, 1.0, fract(motionPX)));
    if (all(lessThan(abs(motionPX), vec2(0.01)))) motionPX = vec2(0.0);
    motion = motionPX / resolution;
    prevCoords.xy = v_texCoords - motion;


    vec4 currentColor = texture(currentBuffer, v_texCoords + 0.5 * jitter);
    vec4 taaColor = currentColor;
    if (all(greaterThanEqual(prevCoords.xy, vec2(0))) && all(lessThan(prevCoords.xy, vec2(1.0)))) {
        vec3 neighborMin, neighborMax;
        neighborhoodMinMax(v_texCoords, 1.0/resolution, neighborMin, neighborMax);

        vec4 historySample = texture(historyBuffer, prevCoords.xy);
        //vec4 historySample = texture(historyBuffer, v_texCoords);
        //historySample.rgb = yccToRGB * clip(neighborMin, neighborMax, rgbToYCC * historySample.rgb);
        //vec3 clampedHistory = clamp(historySample.rgb, neighborMin, neighborMax);
        //historySample.rgb = clampedHistory;
        //if (!all(equal(motionPX, vec2(0.0)))) historySample.rgb = clampedHistory;
        //historySample.rgb = clip(neighborMin, neighborMax, historySample.rgb);
        historySample.rgb = yccToRGB * clamp(rgbToYCC * historySample.rgb, neighborMin, neighborMax);  // clamping seems to give much more stable results than clipping. no idea why


        //float velScale = mix(1.0, 0.0, clamp((length(motionPX) - 4.0) / 12.0, 0.0, 1.0));
        //float velScale = 1.0;

        taaColor = mix(currentColor, historySample, persistence);
        //if (velScale < 0.01) f_result = vec4(1, 0, 0, 1);
        //f_result = historySample;

        //if (all(equal(motionPX, vec2(0.0)))) f_result = mix(f_result, vec4(1.0, 0.0, 0.0, 1.0), 0.5);

        //f_result.rg *= 0.5 * fract(abs(motionPX));
        //f_result.b = 0.0;

        //f_result *= vec4(0.75 + 0.25 * motionPX, 1.0, 1.0);

    }

    //vec4 motionColor = mix(currentColor, texture(historyBuffer, v_texCoords.xy), 0.75);

    float velScale = mix(0.0, 1.0, clamp((length(motionPX) - 4.0) / 12.0, 0.0, 1.0));
    f_result = mix(taaColor, currentColor, velScale);


    //f_motion = vec4(0.5 + 0.5 * motion, 0.0, 1.0);
    //f_motion = vec2(0.0, 1.0);
}
