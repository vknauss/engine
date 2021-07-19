#version 330

in vec3 v_worldDir;
in vec2 v_texCoords;

uniform vec3 worldPos;

uniform vec3 sunDirection;
uniform vec3 sunIntensity;
uniform float sunHalfAngleCos = 0.0001;
uniform float sunBlurAngleCos = 0.002;

uniform float cloudiness = 0.45;
uniform float cloudDensity = 1.0;

uniform float fExtinction = 0.0275;
uniform float fScattering = 0.0375;

uniform float time;

uniform vec2 cloudVelocity = vec2(200, 300);

uniform sampler3D cloudNoiseBase;
uniform sampler3D cloudNoiseDetail;
uniform float cloudNoiseBaseScale = 0.00001378456;
uniform float cloudNoiseDetailScale = 0.0000513456;

uniform vec4 baseNoiseContribution;
uniform vec4 baseNoiseThreshold;
uniform vec4 baseNoiseMinHeight = vec4(0.0, 0.2, 0.2, 0.3);
uniform vec4 baseNoiseMinHeightBlend = vec4(0.4, 0.5, 0.6, 0.7);
uniform vec4 baseNoiseMaxHeight = vec4(0.9, 1.0, 1.0, 1.0);
uniform vec4 baseNoiseMaxHeightBlend = vec4(0.2, 0.05, 0.05, 0.05);

uniform float detailNoiseInfluence = 0.3;

uniform mat4 viewProj;


uniform float rPlanet = 1000000.0; //6371000.0;

uniform float cloudBottom = 2000.0;
uniform float cloudTop = 4000.0;
uniform int cloudSteps = 64;
uniform int shadowSteps = 6;
uniform float shadowDist = 100.0;

// for ray calculation
uniform mat4 inverseView;
uniform vec2 pixelOffset;
uniform vec2 halfScreenSize;

const vec3 bluesky = vec3(0.2, 0.8, 2.0);

layout(location=0) out vec4 f_color;

const float PI = 3.141592653;

const int maxSteps = 256;

// not sure what to call it but lets say temporal ordered dithering? based on
// https://www.shadertoy.com/view/ttcSD8
// I think this originally comes from the guerilla horizon:zero dawn presentation
/*const int bayer4x4[16] = int[16] ( 0,  8,  2, 10,
                                  12,  4, 14,  6,
                                   3, 11,  1,  9,
                                  15,  7, 13,  5);*/
// i actually changed to rendering a 16th-resolution texture instead of skipping 15/16 pixels
// much better performance (~2ms instead of ~7), just upsample in the accumulation pass

//const float maxCloudStartDist = sqrt(2.0*rPlanet*cloudBottom + cloudBottom*cloudBottom);
//const float maxCloudEndDist = sqrt(2.0*rPlanet*cloudTop + cloudTop*cloudTop);
//const float stepDist =  (maxCloudEndDist-maxCloudStartDist) / (cloudSteps-1); //(maxCloudEndDist-maxCloudStartDist) / ((pow(1.5, cloudSteps) - 1) / (1.5 - 1)); //(maxCloudEndDist-maxCloudStartDist) / (cloudSteps-1);

// Formula for distance from a point at the top of a circle with radius r1 to the edge of a circle with radius r2 along a ray with vertical angle theta:
// d = sqrt(r2^2 - r1^2(1 - cos^2(theta))) - r1 * cos(theta)
float distanceToSphereEdge(float r1, float r2, float cst) {
    float r12 = r1*r1;
    float r22 = r2*r2;
    float cs2 = cst*cst;
    return cs2 >= 1.0 - (r22/r12) ? sqrt(r22 - r12*(1.0 - cs2)) - r1*cst : 0.0;
}

// retval = # of intersections (0, 1, or 2)
// t1 = near intersection distance
// t2 = far intersection distance
// o = ray origin, d = ray direction (assumed normalized)
// c = sphere center, r = sphere radius
int raySphereIntersect(vec3 o, vec3 d, vec3 c, float r, out float t1, out float t2) {
    vec3 v = c - o;
    float vv = dot(v, v);
    float vd = dot(v, d);
    float r2 = r * r;
    float desc = vd * vd + r2 - vv;
    if (desc < 0.0 || (desc == 0.0 && vd < 0.0)) return 0;  // no intersection or backwards tangent
    if (desc == 0.0) {  // forwards tangent
        t1 = vd;
        return 1;
    }
    float sqd = sqrt(desc);
    if (r2 - vv >= 0.0) {  // inside or on edge
        t1 = vd + sqd;
        return 1;
    }
    if (vd < -sqd) return 0;  // backwards intersection
    t1 = vd - sqd;
    t2 = vd + sqd;
    return 2;
}

float cloud(vec3 coord, float height) {
    vec4 baseNoise = texture(cloudNoiseBase, coord * cloudNoiseBaseScale * vec3(1, 3, 1));
    baseNoise = clamp((baseNoise - baseNoiseThreshold) / (1.0 - baseNoiseThreshold), 0.0, 1.0);
    baseNoise *= clamp((height - cloudBottom - baseNoiseMinHeight * (cloudTop - cloudBottom)) / (baseNoiseMinHeightBlend * (cloudTop-cloudBottom)), 0.0, 1.0);
    baseNoise *= clamp((cloudBottom + baseNoiseMaxHeight * (cloudTop - cloudBottom) - height) / (baseNoiseMaxHeightBlend * (cloudTop-cloudBottom)), 0.0, 1.0);
    baseNoise *= baseNoiseContribution;
    float detailNoise = texture(cloudNoiseDetail, coord * cloudNoiseDetailScale).r - 1.0;

    float d = baseNoise.r + baseNoise.g + baseNoise.b + baseNoise.a + detailNoiseInfluence * detailNoise;

    d = cloudDensity * clamp((d-1.0+cloudiness)/cloudiness, 0.0, 1.0);

    //d *= clamp((dist - startDist) / 700.0, 0.0, 1.0);
    //d *= clamp((endDist - dist) / 700.0, 0.0, 1.0);

    return d;
}

float phaseHG(float g, float cst) {
    float gg = g*g;
    return (1 - gg) / (4*PI*pow(1+gg-2*g*cst, 1.5));
}

void main() {

    vec4 viewDir = vec4(2.0 * (v_texCoords + pixelOffset - 0.5) * halfScreenSize, -1.0, 0.0);
    vec3 worldDir = normalize(vec3(inverseView * viewDir));

    /*vec2 r = worldDir.xy;
    float dist = sqrt(dot(r, r)) * 30;
    vec3 rainbow = 2 * pow(clamp(vec3(sin(dist + 2 * PI / 3) + 0.5, sin(dist + 4 * PI / 3) + 0.5, sin(dist) + 0.5), vec3(0.0), vec3(1.0)), vec3(2.2));
    f_color = vec4(mix(vec3(0.2, 0.8, 2.0), rainbow, clamp(10*(r.y-0.1), 0.0, 1.0) * (1.0 - clamp(abs(100 - dist*dist)/(6*PI*PI), 0.0, 1.0))) , 1.0);*/

    vec3 planetCenter = vec3(worldPos.x, -rPlanet, worldPos.z);

    float t1Planet, t2Planet;
    int hitPlanet = raySphereIntersect(worldPos, worldDir, planetCenter, rPlanet, t1Planet, t2Planet);

    float t1CloudBottom, t2CloudBottom;
    int hitCloudBottom = raySphereIntersect(worldPos, worldDir, planetCenter, rPlanet + cloudBottom, t1CloudBottom, t2CloudBottom);

    float t1CloudTop, t2CloudTop;
    int hitCloudTop = raySphereIntersect(worldPos, worldDir, planetCenter, rPlanet + cloudTop, t1CloudTop, t2CloudTop);

    float cloudStartDist, cloudEndDist;
    int rayThroughCloud = 0;

    int ccase = 0;
    if (hitCloudTop == 2) {  // Fully above the cloud layer, trace back towards the planet
        cloudStartDist = t1CloudTop;
        if (hitPlanet == 0 || t2CloudTop < t1Planet)
            cloudEndDist = t2CloudTop;
        else
            cloudEndDist = t1CloudBottom;
        rayThroughCloud = 1;
        ccase = 1;
    } else if (hitCloudBottom == 2) {  // We are inside the the cloud layer, only trace the distance to the nearest edge
        cloudStartDist = 0.0;

        if (hitPlanet == 0 || t1CloudTop < t1Planet) {
            cloudEndDist = t1CloudTop;
        ccase = 2;
        } else {
            cloudEndDist = t1CloudBottom;
        ccase = 5;
        }
        rayThroughCloud = 1;
    } else if (hitCloudBottom == 1) {  // We are underneath the cloud layer
        cloudStartDist = t1CloudBottom;
        cloudEndDist = t1CloudTop;
        rayThroughCloud = 1;
        ccase = 3;
    } else if (hitCloudTop == 1) {  // Again inside cloud layer, except looking out towards the top instead
        cloudStartDist = 0.0;
        cloudEndDist = t1CloudTop;
        rayThroughCloud = 1;
        ccase = 4;
    }  // otherwise no intersection

    vec3 bgColor;

    if (hitPlanet == 2) { // Looking at ground
        bgColor = vec3(0.2);
    } else { // Looking at sky
        float sunAngleCos = 1.0 + dot(worldDir, sunDirection);
        float sunFactor = max((sunAngleCos - sunBlurAngleCos) / (sunHalfAngleCos - sunBlurAngleCos), 0.0);
        sunFactor *= sunFactor;

        vec3 sunColor = mix(min(sunIntensity, vec3(1.0)), sunIntensity, float(sunFactor >= 1.0));

        bgColor = mix(bluesky, sunColor, min(sunFactor, 1.0));
        //bgColor = vec3(ccase&1, (ccase&2)>>1, (ccase&4)>>2);
    }

    if (rayThroughCloud == 1 && (hitPlanet < 2 || cloudStartDist < t1Planet)) {  // Looking through clouds
        float cloudStepDist = clamp((cloudEndDist - cloudStartDist) / (cloudSteps - 1), 10.0, 300.0);
        int steps = min(int((cloudEndDist - cloudStartDist) / cloudStepDist + 1), maxSteps);

        float cosTheta = -dot(worldDir, sunDirection);
        float phaseSun = 0.01 * phaseHG(0.8, cosTheta) + 0.8 * phaseHG(0.1, cosTheta) + 0.19 * phaseHG(-0.2, cosTheta);

        float phaseAmbient = 1.0 / (4.0 * PI);

        vec3 cloudLuminance = vec3(0, 0, 0);
        float cloudExtinction = 1.0;
        float sumDensity = 0.0;

        float cloudDist = cloudStartDist;

        //if (cloudStepDist < 0.0) bgColor = vec3(1, 0, 0);

        int inCloud = 0;

        for (int i = 0; i < steps; ++i) {
            vec3 cloudPos = worldPos + worldDir * cloudDist + vec3(cloudVelocity.x, 0, cloudVelocity.y) * time;

            float density = cloud(cloudPos, length(worldPos + worldDir * cloudDist - planetCenter) - rPlanet);

            if (density > 0.0) {
                inCloud = 2;
            } else {
                --inCloud;
            }
            if (inCloud > 0) {
                float sunExtinction = 1.0;
                for (int j = 0; j < shadowSteps; ++j) {
                    vec3 shadowPos = cloudPos - sunDirection * shadowDist * (j+1);

                    float shadowCloudDensity = cloud(shadowPos, length(worldPos + worldDir * cloudDist - sunDirection * shadowDist * (j+1) - planetCenter) - rPlanet);

                    sunExtinction *= exp(-fExtinction * shadowCloudDensity * shadowDist);
                }

                vec3 ambientColor = sunIntensity;
                vec3 sunColor = sunExtinction * sunIntensity;

                cloudExtinction *= exp(-fExtinction * density * cloudStepDist);
                if (cloudExtinction <= 0.001) {
                    cloudExtinction = 0.0;
                    break;
                }

                vec3 sampleLuminance = cloudExtinction * fScattering * density * cloudStepDist * (phaseAmbient * ambientColor + phaseSun * sunColor);// + vec3(float(i+1)/cloudSteps, 0, 0);
                cloudLuminance += clamp(1.0 -sumDensity, 0.0, 1.0) * sampleLuminance;
            }

            cloudDist += cloudStepDist;

            sumDensity += density;
        }

        float distBlend = 1.0 - clamp((cloudStartDist-cloudBottom)/500000.0, 0.0, 1.0);
        distBlend *= distBlend;
        bgColor = distBlend * cloudLuminance + bgColor * (cloudExtinction  + (1.0 - cloudExtinction) * (1.0 - distBlend));
    }

    f_color = vec4(bgColor, 1.0);
}
