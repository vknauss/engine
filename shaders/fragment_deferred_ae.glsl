
#ifdef ENABLE_MSAA
uniform sampler2DMS gBufferPositionViewSpace;
uniform sampler2DMS gBufferNormalViewSpace;
uniform sampler2DMS gBufferAlbedoMetallic;
uniform sampler2DMS gBufferEmissionRoughness;
#else
//uniform sampler2D gBufferPositionViewSpace;
//uniform sampler2D gBufferNormalViewSpace;
uniform sampler2D gBufferAlbedoMetallic;
uniform sampler2D gBufferEmissionRoughness;
//uniform sampler2D gBufferDepth;
#endif // ENABLE_MSAA

uniform int enableSSAO;
uniform sampler2D ssaoMap;

uniform vec3 ambientIntensity;
uniform float ambientPower;


in vec2 v_texCoords;

layout(location=0) out vec4 out_color;

const float gamma = 2.2;
const int samples = 4;

const float PI = 3.14159265;


// Compute the SSD of all samples for the fragment
// return 1 if SSD above threshold, 0 otherwise
// This assumes 3-component data
int isEdge(sampler2DMS msSampler, float threshold) {
    ivec2 texCoordsMS = ivec2(gl_FragCoord.xy);
    vec3 positions[samples];
    vec3 mean = vec3(0.0, 0.0, 0.0);
    for(int i = 0; i < samples; i++) {
        positions[i] = texelFetch(msSampler, texCoordsMS, i).xyz;
        mean += positions[i];
    }
    mean /= float(samples);
    float ssd = 0.0;
    for(int i = 0; i < samples; i++) {
        positions[i] -= mean;
        ssd += dot(positions[i], positions[i]);
    }
    return int(ssd > threshold);
}

void main() {
    float ssaoAmbient = (enableSSAO == 0) ? 1.0 : texture(ssaoMap, v_texCoords).r;

    #ifdef ENABLE_MSAA
    // Performance could possibly be increased by stencil buffering edge detection
    // then doing two shader passes, one single-sampled and one multi-sampled.
    // The method done here is simpler to implement but requires a dynamic branch
    ivec2 texCoordsMS = ivec2(gl_FragCoord.xy);
    if((isEdge(gBufferPositionViewSpace, 0.001) | isEdge(gBufferNormalViewSpace, 0.005)) == 1) {
        out_color = vec4(0.0, 0.0, 0.0, 0.0);
        for(int i = 0; i < samples; i++) {
            vec4 positionViewSpace = texelFetch(gBufferPositionViewSpace, texCoordsMS, i).xyzw;
            vec4 albedoMetallic    = texelFetch(gBufferAlbedoMetallic,    texCoordsMS, i).rgba;
            vec4 emissionRoughness = texelFetch(gBufferEmissionRoughness, texCoordsMS, i).rgba;

            vec3     albedo = albedoMetallic.rgb;
            vec3   emission = emissionRoughness.rgb;
            float  metallic = albedoMetallic.w;

            vec3 ambient = ambientIntensity * (1.0 - metallic) * pow(ssaoAmbient, ambientPower);

            vec4 sampleColor = vec4(emission + ambient * albedo, positionViewSpace.w);
            out_color.rgb += (sampleColor.a > 0.0) ? sampleColor.a * sampleColor.rgb : vec3(0.0);
            out_color.a   += sampleColor.a;
        }
        out_color.rgb /= (out_color.a > 0.0) ? out_color.a : 1.0;
        out_color.a /= float(samples);
    } else {
        vec4 positionViewSpace = texelFetch(gBufferPositionViewSpace, texCoordsMS, 0).xyzw;
        vec4 albedoMetallic    = texelFetch(gBufferAlbedoMetallic,    texCoordsMS, 0).rgba;
        vec4 emissionRoughness = texelFetch(gBufferEmissionRoughness, texCoordsMS, 0).rgba;

        vec3     albedo = albedoMetallic.rgb;
        vec3   emission = emissionRoughness.rgb;
        float  metallic = albedoMetallic.w;

        vec3 ambient = ambientIntensity * (1.0 - metallic) * pow(ssaoAmbient, ambientPower);

        out_color = vec4(emission + ambient * albedo, positionViewSpace.w);
    }
    #else

    vec4 albedoMetallic    = texture(gBufferAlbedoMetallic,    v_texCoords).rgba;
    vec4 emissionRoughness = texture(gBufferEmissionRoughness, v_texCoords).rgba;

    vec3     albedo = albedoMetallic.rgb;
    vec3   emission = emissionRoughness.rgb;
    float  metallic = albedoMetallic.w;

    vec3 ambient = ambientIntensity * (1.0 - metallic) * pow(ssaoAmbient, ambientPower);

    //out_color = (positionViewSpace.w == 0.0) ? vec4(0) : vec4(emission + ambient * albedo, 1.0);
    //out_color = vec4((positionViewSpace.w == 0.0) ? vec3(0) : emission + ambient * albedo, 1.0);
    out_color = vec4(emission + ambient * albedo, 1.0);

    #endif // ENABLE_MSAA
}
