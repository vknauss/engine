#version 400

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

in mat3 tbnViewSpace;
in vec4 positionViewSpace;
in vec2 texCoords;


layout(location=0) out vec4 gBufferPositionViewSpace;
layout(location=1) out vec3 gBufferNormalViewSpace;
layout(location=2) out vec4 gBufferAlbedoMetallic;
layout(location=3) out vec4 gBufferEmissionRoughness;

const float gamma = 2.2;


mat3 orthonormalize(mat3 m) {
    vec3 v2 = normalize(m[2]);
    vec3 v0 = normalize(m[0] - v2 * dot(m[0], v2));
    vec3 v1 = cross(v2, v0);
    return mat3(v0, v1, v2);
}


void main() {
    gBufferPositionViewSpace = positionViewSpace;

    mat3 tbn = orthonormalize(tbnViewSpace);
    vec3 normal = tbn[2];
    if (useNormalsTexture == 1) {
        normal = 2.0 * texture(normalsTexture, texCoords).xyz - 1.0;
        normal = tbn * normal;
    }
    normal = normalize(normal);
    gBufferNormalViewSpace.xyz = 0.5 * normal + 0.5;


    vec3 albedo = color;
    if (useDiffuseTexture == 1) {
        vec4 diffuse = texture(diffuseTexture, texCoords);
        albedo *= diffuse.rgb;
        if(diffuse.a == 0.0) discard;
    }
    albedo = pow(albedo, vec3(gamma));

    vec3 em = emission;
    if (useEmissionTexture == 1) {
        em *= texture(emissionTexture, texCoords).rgb;
    }

    vec2 mr = vec2(metallic, roughness);
    if (useMetallicRoughnessTexture == 1) {
        mr *= texture(metallicRoughnessTexture, texCoords).bg;
    }

    gBufferAlbedoMetallic    = vec4(albedo, mr.x);
    gBufferEmissionRoughness = vec4(em, mr.y);
}
