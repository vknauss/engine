
layout(location=0) out vec4 out_color;

uniform vec3 lightPositionViewSpace;
uniform vec3 lightIntensity;

uniform float minIntensity;

#ifdef ENABLE_SHADOW
uniform mat4 inverseView;
uniform mat4 cubeFaceMatrices[6];
uniform samplerCube shadowMap;

uniform float lightBleedCorrectionBias;
uniform float lightBleedCorrectionPower;

uniform int enableEVSM;

//const float cPos = 42.0;
//const float cNeg = 14.0;

const float cPos = 1.0;
const float cNeg = 1.0;
#endif // ENABLE_SHADOW

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

const float gamma = 2.2;
const int samples = 4;
const float PI = 3.14159265;


const mat4 coordTransform = mat4(
    0.5, 0.0, 0.0, 0.0,
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.5, 0.5, 0.5, 1.0);

#ifdef ENABLE_SHADOW
float chebyshev(float value, vec2 moments, float minVariance) {
    float variance = max(moments.y - moments.x * moments.x, minVariance);
    float d = value - moments.x;

    return max(float(d <= 0.0), variance / (variance + d*d));
}

float computeVisibleVSM(float depth, vec2 moments) {
    return chebyshev(depth, moments, 0.01);
}

float computeVisibleEVSM(float depth, vec4 moments) {
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

vec2 sampleMomentsVSM(vec3 dir) {
    // first and second depth moments approximately stored in texture
    vec2 moments = texture(shadowMap, dir).xy;
    return moments;
}

vec4 sampleMomentsEVSM(vec3 dir) {
    // xy = first and second moments under positive warp
    // zw = first and second moments under negative warp
    vec4 moments = texture(shadowMap, dir);

    return moments;
}

float sampleVisibleSimple(float depth, vec3 dir) {
    float sampleDepth = texture(shadowMap, dir).x;
    float visible = float(depth - 0.05 <= sampleDepth);
    return visible;
}

float sampleVisible(float depth, vec3 dir) {
    float visible = 0.0;

    if(enableEVSM == 1) {
        visible = computeVisibleEVSM(depth, sampleMomentsEVSM(dir));
    } else {
        visible = computeVisibleVSM(depth, sampleMomentsVSM(dir));
    }

    // hack to reduce light bleed, from GPU gems 3 chapter 8
    visible = clamp((visible - lightBleedCorrectionBias) / (1.0 - lightBleedCorrectionBias), 0.0, 1.0);
    visible = pow(visible, lightBleedCorrectionPower);

    return visible;
}
#endif // ENABLE_SHADOW

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
    //float ddn = dot(direction, normal);

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

    float d = distributionGGX(normal, halfway, roughness);
    vec3  f = fresnelSchlick(f0, halfway, directionToView);
    float g = geometrySmith(directionToView, directionToLight, normal, alpha);

    // Conservation of energy + metallic surfaces have no diffuse component
    vec3 kD  = 1.0 - f;
         kD *= 1.0 - metallic;

    float dnl = max(dot(normal, directionToLight), 0.0);
    float dnv = max(dot(normal, directionToView),  0.0);
    float denom = max(4.0 * dnl * dnv, 0.0001);

    vec3 diffuse  = kD * albedo / PI;
    vec3 specular = d * f * g / denom;

    vec3 color  = dnl * lightIntensity * (diffuse + specular);

    return color;
}

vec4 computeLightingAndShading(vec4 positionViewSpace, vec3 normalViewSpace, vec3 albedo, float roughness, float metallic) {
    vec3 fromLight = positionViewSpace.xyz - lightPositionViewSpace;

    vec3 directionToView  = normalize(-positionViewSpace.xyz);
    vec3 directionToLight = normalize(-fromLight);

    vec3 intensity = max(vec3(0.0), lightIntensity / (1.0 + dot(fromLight, fromLight)) - minIntensity) / (1.0 - minIntensity);

    vec3 color = cookTorranceBRDF(directionToView, directionToLight, normalViewSpace, intensity, albedo, roughness, metallic);

    //float attenuation = min(1.0, max(0.0, (1.0 / dot(fromLight, fromLight)) - minIntensity) / (1.0 - minIntensity));
    //float attenuation = max(0.0, (1.0 / (1.0 + dot(fromLight, fromLight)) - minIntensity)) / (1.0 - minIntensity);
    //float attenuation = 1.0 / (1.0 + dot(fromLight, fromLight));

    float visible = 1.0;

    #ifdef ENABLE_SHADOW
    fromLight = vec3(inverseView * vec4(fromLight, 0.0));

    vec4 positionWorldSpace = inverseView * positionViewSpace;

    float dx = abs(fromLight.x), dy = abs(fromLight.y), dz = abs(fromLight.z);
    float maxd = max(dx, max(dy, dz));

    vec4 shadowClip;
    if(maxd == dx) {
        if(fromLight.x > 0) {
            shadowClip = cubeFaceMatrices[0] * positionWorldSpace;
        } else {
            shadowClip = cubeFaceMatrices[1] * positionWorldSpace;
        }
    } else if(maxd == dy) {
        if(fromLight.y > 0) {
            shadowClip = cubeFaceMatrices[2] * positionWorldSpace;
        } else {
            shadowClip = cubeFaceMatrices[3] * positionWorldSpace;
        }
    } else {
        if(fromLight.z > 0) {
            shadowClip = cubeFaceMatrices[4] * positionWorldSpace;
        } else {
            shadowClip = cubeFaceMatrices[5] * positionWorldSpace;
        }
    }
    float depth = shadowClip.z / shadowClip.w;
    depth = 0.5 * depth + 0.5;

    visible = sampleVisible(depth, fromLight);
    //visible = 1.0;

    //color = textureCube(shadowMap, fromLight).rgb;

    #endif // ENABLE_SHADOW

    //return vec4(visible * attenuation * color, positionViewSpace.w);

    //return (positionViewSpace.w == 0.0) ? vec4(0) : vec4(visible * color, 1.0);
    //return vec4((positionViewSpace.w == 0.0) ? vec3(0) : visible * color, 1.0);
    return vec4(visible * color, 1.0);
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
    ivec2 texCoordsMS = ivec2(gl_FragCoord.xy);
    if((isEdge(gBufferPositionViewSpace, 0.001) | isEdge(gBufferNormalViewSpace, 0.005)) == 1) {
        out_color = vec4(0.0, 0.0, 0.0, 0.0);
        for(int i = 0; i < samples; i++) {
            vec4 positionViewSpace = texelFetch(gBufferPositionViewSpace, texCoordsMS, i).xyzw;
            vec3 normalViewSpace   = texelFetch(gBufferNormalViewSpace,   texCoordsMS, i).xyz;
            vec4 albedoMetallic    = texelFetch(gBufferAlbedoMetallic,    texCoordsMS, i).rgba;
            vec4 emissionRoughness = texelFetch(gBufferEmissionRoughness, texCoordsMS, i).rgba;

            normalViewSpace = normalize(2.0 * normalViewSpace - 1.0);

            vec3 albedo = albedoMetallic.xyz;
            float metallic = albedoMetallic.w;
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

        vec3 albedo = albedoMetallic.xyz;
        float metallic = albedoMetallic.w;
        float roughness = emissionRoughness.w;

        out_color = computeLightingAndShading(positionViewSpace, normalViewSpace, albedo, roughness, metallic);
    }
    #else
    vec2 v_texCoords = gl_FragCoord.xy / textureSize(gBufferPositionViewSpace, 0).xy;

    vec4 positionViewSpace = texture(gBufferPositionViewSpace, v_texCoords).xyzw;
    vec3 normalViewSpace   = texture(gBufferNormalViewSpace,   v_texCoords).xyz;
    vec4 albedoMetallic    = texture(gBufferAlbedoMetallic,    v_texCoords).rgba;
    vec4 emissionRoughness = texture(gBufferEmissionRoughness, v_texCoords).rgba;

    normalViewSpace = 2.0 * normalViewSpace - 1.0;
    //normalViewSpace = normalize(normalViewSpace);
    //if(normalViewSpace.z < 0) normalViewSpace.z = -normalViewSpace.z;

    vec3 albedo = albedoMetallic.xyz;
    float metallic = albedoMetallic.w;
    float roughness = emissionRoughness.w;


    out_color = computeLightingAndShading(positionViewSpace, normalViewSpace, albedo, roughness, metallic);

    #endif // ENABLE_MSAA
}
