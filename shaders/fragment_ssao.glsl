
in vec2 v_texCoords;

out vec3 o_ambient;

const int ssaoKernelSize = 32;
const int rotationTextureSize = 4;
const float sampleRadius = 0.25;
const int samples = 4;

const mat4 coordTransform = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.5, 0.5, 0.5, 1.0);

uniform vec3 ssaoKernelSamples[ssaoKernelSize];

#ifdef ENABLE_MSAA
uniform sampler2DMS gBufferPositionViewSpace;
uniform sampler2DMS gBufferNormalViewSpace;
#else
uniform sampler2D gBufferPositionViewSpace;
uniform sampler2D gBufferNormalViewSpace;
#endif // ENABLE_MSAA

uniform sampler2D randomRotationTexture;

uniform mat4 projection;

float computeAO(vec3 positionViewSpace, vec3 normalViewSpace, vec3 randomRotation) {
    // compute tangent-to-view space transform
    vec3 tangent = normalize(randomRotation - normalViewSpace * dot(randomRotation, normalViewSpace));
    vec3 bitangent = cross(normalViewSpace, tangent);
    mat3 tangentToView = mat3(tangent, bitangent, normalViewSpace);

    float occlusion = 0.0;
    for(int i = 0; i < ssaoKernelSize; i++) {
        vec3 sampleViewSpace = tangentToView * ssaoKernelSamples[i];
        sampleViewSpace = positionViewSpace + sampleRadius * sampleViewSpace;

        vec4 sampleClipSpace = projection * vec4(sampleViewSpace, 1.0);
        sampleClipSpace /= sampleClipSpace.w;
        sampleClipSpace = coordTransform * sampleClipSpace;

        #ifdef ENABLE_MSAA
        vec4 samplePosition = texelFetch(gBufferPositionViewSpace, ivec2(sampleClipSpace.xy * textureSize(gBufferPositionViewSpace)), 0);
        #else
        vec4 samplePosition = texture(gBufferPositionViewSpace, sampleClipSpace.xy);
        #endif // ENABLE_MSAA

        //occlusion += max(samplePosition.z - sampleViewSpace.z, 0.0) / sampleRadius * float(abs(samplePosition.z - positionViewSpace.z) <= sampleRadius) * samplePosition.w;
        occlusion += float(samplePosition.z - sampleViewSpace.z > 0.0) * float(abs(samplePosition.z - positionViewSpace.z) <= sampleRadius) * samplePosition.w;
    }

    occlusion /= float(ssaoKernelSize);
    return min(1.0, occlusion);
}

void main() {
    #ifdef ENABLE_MSAA
    ivec2 texCoordsMS = ivec2(v_texCoords.xy * textureSize(gBufferPositionViewSpace).xy);
    vec4 positionViewSpace = texelFetch(gBufferPositionViewSpace, texCoordsMS, 0);
    vec3 normalViewSpace = normalize(2.0 * texelFetch(gBufferNormalViewSpace, texCoordsMS, 0).xyz - vec3(1.0));
    #else
    vec4 positionViewSpace = texture(gBufferPositionViewSpace, v_texCoords);
    vec3 normalViewSpace = normalize(2.0 * texture(gBufferNormalViewSpace, v_texCoords).xyz - vec3(1.0));
    #endif // ENABLE_MSAA
    vec3 rotation = texture(randomRotationTexture, gl_FragCoord.xy / float(rotationTextureSize)).xyz;

    if(positionViewSpace.w == 0.0) discard;
    o_ambient = vec3(1.0 - computeAO(positionViewSpace.xyz, normalViewSpace, rotation));

}
