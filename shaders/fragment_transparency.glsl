#version 330

// Based on Weighted Blended Order Independent Transparency by Morgan McGuire
// Implementation code from:
// http://casual-effects.blogspot.com/2015/03/implemented-weighted-blended-order.html


uniform vec3 color;
uniform float roughness;
uniform float metallic;
uniform vec3 emission;

uniform int useDiffuseTexture;
uniform sampler2D diffuseTexture;

uniform int useNormalsTexture;
uniform sampler2D normalsTexture;

uniform int useEmissionTexture;
uniform sampler2D emissionTexture;

uniform int useMetallicRoughnessTexture;
uniform sampler2D metallicRoughnessTexture;

uniform float alpha;

uniform mat4 inverseProjection;

uniform vec3 lightDirection;
uniform vec3 lightIntensity;

uniform vec3 ambientLight;

in mat3 tbnViewSpace;
in vec2 texCoords;

in vec4 v_clipPos;

flat in uint uid;

layout(location=0) out vec4 f_accum;
layout(location=1) out float f_revealage;

const float gamma = 2.2;
const float PI = 3.1415926535;

mat3 orthonormalize(mat3 m) {
    vec3 v2 = normalize(m[2]);
    vec3 v0 = normalize(m[0] - v2 * dot(m[0], v2));
    vec3 v1 = cross(v2, v0);
    return mat3(v0, v1, v2);
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

vec3 cookTorranceBRDF(vec3 directionToView, vec3 directionToLight, vec3 normal, vec3 lightIntensity, vec3 albedo, float roughness, float metallic, float transparency, out vec3 transmission) {
    vec3      f0 = mix(vec3(0.04), albedo, metallic);  // Approximation presented by Joey de Vries, https://learnopengl.com/PBR/Theory
    float  alpha = roughness*roughness;
    vec3 halfway = normalize(directionToView + directionToLight);

    //float d = distributionGGX(normal, halfway, alpha);
    float d = distributionGGX(normal, halfway, roughness);
    vec3  f = fresnelSchlick(f0, halfway, directionToView);
    float g = geometrySmith(directionToView, directionToLight, normal, alpha);

    // Conservation of energy + metallic surfaces have no diffuse component
    vec3 kTransparency = transparency * (1.0 - f);
    vec3 kD  = 1.0 - f - kTransparency;
         kD *= 1.0 - metallic;

    float dnl = max(dot(normal, directionToLight), 0.0);
    float dnv = max(dot(normal, directionToView),  0.0);
    float denom = max(4.0 * dnv * dnl, 0.0001);

    vec3 diffuse  = kD * albedo / PI;
    vec3 specular = d * f * g / denom;

    transmission = kTransparency * albedo;

    vec3 color  = dnl * lightIntensity * (diffuse + specular);

    return color;
}

vec4 computeLightingAndShading(vec4 positionViewSpace, vec3 normalViewSpace, vec3 albedo, float roughness, float metallic, float transparency, out vec3 transmission) {
    vec3 directionToView  = normalize(-positionViewSpace.xyz);
    vec3 directionToLight = normalize(-lightDirection);

    vec3 color = cookTorranceBRDF(directionToView, directionToLight, normalViewSpace, lightIntensity, albedo, roughness, metallic, transparency, transmission);

    //float visible = (enableShadows == 1) ? computeVisible(positionViewSpace) : 1.0;

    return vec4(color, 1.0);
}

void computeTransparency(vec4 premultipliedReflect, vec3 transmit, float csZ, out vec4 accum, out float revealage) {
    /* Modulate the net coverage for composition by the transmission. This does not affect the color channels of the
       transparent surface because the caller's BSDF model should have already taken into account if transmission modulates
       reflection. This model doesn't handled colored transmission, so it averages the color channels. See

          McGuire and Enderton, Colored Stochastic Shadow Maps, ACM I3D, February 2011
          http://graphics.cs.williams.edu/papers/CSSM/

       for a full explanation and derivation.*/

    premultipliedReflect.a *= 1.0 - clamp((transmit.r + transmit.g + transmit.b) * (1.0 / 3.0), 0, 1);

    /* You may need to adjust the w function if you have a very large or very small view volume; see the paper and
       presentation slides at http://jcgt.org/published/0002/02/09/ */
    // Intermediate terms to be cubed
    float a = min(1.0, premultipliedReflect.a) * 8.0 + 0.01;
    float b = -gl_FragCoord.z * 0.95 + 1.0;

    /* If your scene has a lot of content very close to the far plane,
       then include this line (one rsqrt instruction):
       b /= sqrt(1e4 * abs(csZ)); */
    float w   = clamp(a * a * a * 1e8 * b * b * b, 1e-2, 3e2);
    accum     = premultipliedReflect * w;
    revealage = premultipliedReflect.a;
}

void main() {

    vec4 viewPos = inverseProjection * v_clipPos;

    mat3 tbn = orthonormalize(tbnViewSpace);
    vec3 normal = tbn[2];
    if (useNormalsTexture == 1) {
        normal = 2.0 * texture(normalsTexture, texCoords).xyz - 1.0;
        normal = tbn * normal;
        normal = normalize(normal);
    }
    normal = normalize(normal);

    vec3 albedo = color;
    float a = alpha;
    if (useDiffuseTexture == 1) {
        vec4 diffuse = texture(diffuseTexture, texCoords);
        albedo *= diffuse.rgb;
        a *= diffuse.a;
    }
    if (alpha == 1.0) discard;  // rendered in the opaque pass

    albedo = pow(albedo, vec3(gamma));

    vec3 em = emission;
    if (useEmissionTexture == 1) {
        em *= texture(emissionTexture, texCoords).rgb;
    }

    float metal = metallic;
    float rough = roughness;
    if (useMetallicRoughnessTexture == 1) {
        vec2 mr = texture(metallicRoughnessTexture, texCoords).bg;
        metal *= mr.x;
        rough *= mr.y;
    }

    vec3 transmission;
    vec4 brdfColor = computeLightingAndShading(viewPos, normal, albedo, rough, metal, 1.0 - a, transmission);

    computeTransparency(brdfColor + vec4(em + a * ambientLight * albedo, 0.0), transmission, v_clipPos.z, f_accum, f_revealage);
}
