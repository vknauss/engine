
#define MAX_CASCADES 15

uniform vec3 lightDirectionViewSpace;
uniform vec3 lightIntensity;

uniform sampler2DArray shadowMap;

uniform mat4 viewToLightMatrix;
uniform mat4 shadowProjectionMatrices[MAX_CASCADES];
uniform int numShadowCascades;
uniform float cascadeSplitDepths[MAX_CASCADES];
uniform float cascadeBlurRanges[MAX_CASCADES];
uniform float lightBleedCorrectionBias;
uniform float lightBleedCorrectionPower;

#ifdef ENABLE_MSAA
uniform sampler2DMS gBufferPositionViewSpace;
uniform sampler2DMS gBufferNormalViewSpace;
uniform sampler2DMS gBufferAlbedoMetallic;
uniform sampler2DMS gBufferEmissionRoughness;
#else
uniform sampler2D gBufferPositionViewSpace;
uniform sampler2D gBufferNormalViewSpace;
uniform sampler2D gBufferAlbedoMetallic;
uniform sampler2D gBufferEmissionRoughness;
#endif // ENABLE_MSAA

uniform int enableEVSM;

in vec2 v_texCoords;

layout(location=0) out vec4 out_color;

const float gamma = 2.2;
const int samples = 4;

const float PI = 3.14159265;

const float cPos = 42.0;
const float cNeg = 14.0;

const mat4 coordTransform = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.5, 0.5, 0.5, 1.0);

float chebyshev(float value, vec2 moments, float minVariance) {
    float variance = max(moments.y - moments.x * moments.x, minVariance);
    float d = value - moments.x;

    return max(float(d <= 0.0), variance / (variance + d*d));
}

float sampleVisibleVSM(float depth, vec2 coords, int cascadeInd) {
    // first and second depth moments approximately stored in texture
    // Unfortunately the deferred method seems to have trouble choosing the right
    // mipmap level, even with the continuously defined light-space derivatives
    vec2 moments = textureLod(shadowMap, vec3(coords, cascadeInd), 0).xy;

    return chebyshev(depth, moments, 0.01);
}

float sampleVisibleEVSM(float depth, vec2 coords, int cascadeInd) {
    // first and second depth moments approximately stored in texture
    vec4 moments;

    moments = textureLod(shadowMap, vec3(coords, cascadeInd), 0).xyzw;

    // compute depth warps pos and neg
    float dp = exp(cPos * depth);
    float dn = -exp(-cNeg * depth);

    // compute min variance for pos and neg warps
    float msp = cPos * dp * 0.001;
    msp = msp * msp;
    float msn = cNeg * dn * 0.001;
    msn = msn * msn;

    return min(chebyshev(dp, moments.xy, msp), chebyshev(dn, moments.zw, msn));
}

float sampleVisibleSimple(float depth, vec2 coords, int cascadeInd) {
    float sampleDepth = textureLod(shadowMap, vec3(coords, cascadeInd), 0).x;

    float visible = float(depth - 0.05 <= sampleDepth);
    return visible;
}


float sampleVisible(float depth, vec2 coords, int cascadeInd) {
    float visible = 0.0;

    if(enableEVSM == 1) {
        visible = sampleVisibleEVSM(depth, coords, cascadeInd);
    } else {
        visible = sampleVisibleVSM(depth, coords, cascadeInd);
    }

    return visible;
}

float adjustVisibility(float visible) {
    // hack to reduce light bleed, from GPU gems 3 chapter 8
    visible = clamp((visible - lightBleedCorrectionBias) / (1.0 - lightBleedCorrectionBias), 0.0, 1.0);
    visible = pow(visible, lightBleedCorrectionPower);

    visible = max(visible - 0.5, 0.0);

    return visible;
}

// Shadow map calculations
float computeVisible(vec4 positionViewSpace) {
    vec4 lightspaceCoords = viewToLightMatrix * positionViewSpace;
    float depth = -lightspaceCoords.z;

    int cascadeInd = 0;
    int inBlurBand = 0;
    float blurMix = 0.0;

    // Select cascade based on distance from camera and uniform-specified depth intervals
    float viewSpaceDepth = -positionViewSpace.z;
    for(int i = 0; i < numShadowCascades; i++) {
        if(viewSpaceDepth >= cascadeSplitDepths[i]) {
            cascadeInd++;
        } else {
            if(i < numShadowCascades-1 && viewSpaceDepth >= cascadeSplitDepths[i] - cascadeBlurRanges[i]) {
                inBlurBand = 1;
                blurMix = 1.0 - ((cascadeSplitDepths[i] - viewSpaceDepth) / cascadeBlurRanges[i]);
            }
            break;
        }
    }
    float visible = 1.0;

    //if (cascadeInd < numShadowCascades) {
        vec4 shadowCoords = shadowProjectionMatrices[cascadeInd] * lightspaceCoords;
        shadowCoords /= shadowCoords.w;
        shadowCoords = coordTransform * shadowCoords;
        float clipDepthNormalized = shadowCoords.z;

        if(inBlurBand == 0) {
            visible = sampleVisible(clipDepthNormalized, shadowCoords.xy, cascadeInd);
        } else {
            vec4 nextCascadeCoords = shadowProjectionMatrices[cascadeInd+1] * lightspaceCoords;
            nextCascadeCoords /= nextCascadeCoords.w;
            nextCascadeCoords = coordTransform * nextCascadeCoords;
            float nextCDN = nextCascadeCoords.z;
            visible = mix(
                sampleVisible(clipDepthNormalized, shadowCoords.xy, cascadeInd),
                sampleVisible(nextCDN, nextCascadeCoords.xy, cascadeInd+1),
                blurMix);
        }
    //} else {
    //    visible = 1.0;
    //}
    visible = (cascadeInd < numShadowCascades) ? visible : 1.0;

    return adjustVisibility(visible);
}

// BRDF implementation based on reference from
// Coding Labs :: Physically Based Rendering - Cook-Torrance
// http://www.codinglabs.net/article_physically_based_rendering_cook_torrance.aspx

// Geometry function uses Schlick-GGX method as presented by Joey de Vries
// https://learnopengl.com/PBR/Theory

// here alpha is not like the opacity but rather roughness squared as defined
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
// made standard by UE4 I suppose
float distributionGGX(vec3 normal, vec3 halfway, float alpha) {
    float   a2 = alpha*alpha;
    float  dnh = dot(normal, halfway);
    float    x = float(dnh > 0.0);
    float dnh2 = dnh*dnh;
    float  den = dnh2*(a2 - 1.0) + 1.0;
           den = PI*den*den;

    return x*a2/max(den, 0.0001);
}


float geometryPartialGGX(vec3 direction, vec3 normal, float k) {
    float ddn = max(dot(direction, normal), 0.0);

    return ddn/max(k + (1.0 - k) * ddn, 0.0001);
}

float geometrySmith(vec3 view, vec3 light, vec3 normal, float alpha) {
    float k = alpha + 1.0;
          k = k*k/8.0;

    return geometryPartialGGX(view, normal, k) * geometryPartialGGX(light, normal, k);
}

vec3 fresnelSchlick(vec3 f0, vec3 halfway, vec3 view) {
    float dvh = max(dot(view, halfway), 0.0);

    return f0 + (1.0 - f0)*pow(max(0.0, 1.0 - dvh), 5.0);
}

vec3 cookTorranceBRDF(vec3 directionToView, vec3 directionToLight, vec3 normal, vec3 lightIntensity, vec3 albedo, float roughness, float metallic) {
    vec3      f0 = mix(vec3(0.04), albedo, metallic);  // Approximation presented by Joey de Vries, https://learnopengl.com/PBR/Theory
    float  alpha = roughness*roughness;
    vec3 halfway = normalize(directionToView + directionToLight);

    //float d = distributionGGX(normal, halfway, alpha);
    float d = distributionGGX(normal, halfway, roughness);
    vec3  f = fresnelSchlick(f0, halfway, directionToView);
    float g = geometrySmith(directionToView, directionToLight, normal, alpha);

    // Conservation of energy + metallic surfaces have no diffuse component
    vec3 kD  = 1.0 - f;
         kD *= 1.0 - metallic;

    float dnl = max(dot(normal, directionToLight), 0.0);
    float dnv = max(dot(normal, directionToView),  0.0);
    float denom = max(4.0 * dnv * dnl, 0.0001);

    vec3 diffuse  = kD * albedo / PI;
    vec3 specular = d * f * g / denom;

    vec3 color  = dnl * lightIntensity * (diffuse + specular);

    return color;
}

vec4 computeLightingAndShading(vec4 positionViewSpace, vec3 normalViewSpace, vec3 albedo, float roughness, float metallic) {
    vec3 directionToView  = normalize(-positionViewSpace.xyz);
    vec3 directionToLight = normalize(-lightDirectionViewSpace);

    vec3 color = cookTorranceBRDF(directionToView, directionToLight, normalViewSpace, lightIntensity, albedo, roughness, metallic);

    float visible = computeVisible(positionViewSpace);

    //return (positionViewSpace.w == 0.0) ? vec4(0) : vec4(visible * color, 1.0);
    return vec4(visible * color, 1.0);
    //return vec4((positionViewSpace.w == 0.0) ? vec3(0) : visible * color, 1.0);
}


#ifdef ENABLE_MSAA
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
#endif // ENABLE_MSAA

void main() {
    #ifdef ENABLE_MSAA
    // Performance could possibly be increased by stencil buffering edge detection
    // then doing two shader passes, one single-sampled and one multi-sampled.
    // The method done here is simpler to implement but requires a dynamic branch
    ivec2 texCoordsMS = ivec2(gl_FragCoord.xy);
    if((isEdge(gBufferPositionViewSpace, 0.001) | isEdge(gBufferNormalViewSpace, 0.005)) == 1) {
        out_color = vec4(0.0, 0.0, 0.0, 0.0);
        for(int i = 0; i < samples; i++) {
            vec4 positionViewSpace = texelFetch(gBufferPositionViewSpace, texCoordsMS, i).xyzw;
            vec3 normalViewSpace   = texelFetch(gBufferNormalViewSpace,   texCoordsMS, i).xyz;
            vec4 albedoMetallic    = texelFetch(gBufferAlbedoMetallic,    texCoordsMS, i).rgba;
            vec4 emissionRoughness = texelFetch(gBufferEmissionRoughness, texCoordsMS, i).rgba;

            normalViewSpace = normalize(2.0 * normalViewSpace - 1.0);

            vec3     albedo = albedoMetallic.rgb;
            float  metallic = albedoMetallic.w;
            float roughness = emissionRoughness.w;

            vec4 sampleColor = computeLightingAndShading(positionViewSpace, normalViewSpace, albedo, roughness, metallic);
            out_color.rgb += (sampleColor.a > 0.0) ? sampleColor.a * sampleColor.rgb : vec3(0.0);
            out_color.a   += sampleColor.a;
        }
        out_color.rgb /= (out_color.a > 0.0) ? out_color.a : 1.0;
        out_color.a /= float(samples);
    } else {
        vec4 positionViewSpace = texelFetch(gBufferPositionViewSpace, texCoordsMS, 0).xyzw;
        vec3 normalViewSpace   = texelFetch(gBufferNormalViewSpace,   texCoordsMS, 0).xyz;
        vec4 albedoMetallic    = texelFetch(gBufferAlbedoMetallic,    texCoordsMS, 0).rgba;
        vec4 emissionRoughness = texelFetch(gBufferEmissionRoughness, texCoordsMS, 0).rgba;

        normalViewSpace = normalize(2.0 * normalViewSpace - 1.0);

        vec3     albedo = albedoMetallic.rgb;
        float  metallic = albedoMetallic.w;
        float roughness = emissionRoughness.w;

        out_color = computeLightingAndShading(positionViewSpace, normalViewSpace, albedo, roughness, metallic);
    }
    #else
    vec4 positionViewSpace = texture(gBufferPositionViewSpace, v_texCoords).xyzw;
    vec3 normalViewSpace   = texture(gBufferNormalViewSpace,   v_texCoords).xyz;
    vec4 albedoMetallic    = texture(gBufferAlbedoMetallic,    v_texCoords).rgba;
    vec4 emissionRoughness = texture(gBufferEmissionRoughness, v_texCoords).rgba;

    normalViewSpace = 2.0 * normalViewSpace - 1.0;
    //normalViewSpace = normalize(normalViewSpace);
    //if(normalViewSpace.z < 0) normalViewSpace.z = -normalViewSpace.z;

    vec3     albedo = albedoMetallic.rgb;
    float  metallic = albedoMetallic.w;
    float roughness = emissionRoughness.w;

    out_color = computeLightingAndShading(positionViewSpace, normalViewSpace, albedo, roughness, metallic);

    #endif // ENABLE_MSAA
}
