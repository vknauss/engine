#version 400

layout(location = 0) out vec4 moments;

in vec2 v_texCoords;

uniform samplerCube depthCubemap;
uniform vec3 faceDirection;
uniform vec3 faceUp;
uniform vec3 faceRight;
uniform int enableEVSM;

//const float cPos = 42.0;
//const float cNeg = 14.0;

const float cPos = 1.0;
const float cNeg = 1.0;

void main() {
    vec2 tc = 2.0 * v_texCoords - 1.0;
    vec3 direction = faceDirection + tc.x * faceRight + tc.y * faceUp;

    float depth = texture(depthCubemap, direction).r;

    vec2 ofs = vec2(1.5) / vec2(textureSize(depthCubemap, 0));

    vec2 tcn = tc + vec2(0, ofs.y);
    vec2 tce = tc + vec2(ofs.x, 0);
    vec2 tcs = tc - vec2(0, ofs.y);
    vec2 tcw = tc - vec2(ofs.x, 0);

    vec3 dirn = faceDirection + tcn.x * faceRight + tcn.y * faceUp;
    vec3 dire = faceDirection + tce.x * faceRight + tce.y * faceUp;
    vec3 dirs = faceDirection + tcs.x * faceRight + tcs.y * faceUp;
    vec3 dirw = faceDirection + tcw.x * faceRight + tcw.y * faceUp;

    float dn = texture(depthCubemap, dirn).r;
    float de = texture(depthCubemap, dire).r;
    float ds = texture(depthCubemap, dirs).r;
    float dw = texture(depthCubemap, dirw).r;

    vec4 m1s = vec4(dn, de, ds, dw);

    //vec4 m1s = textureGather(depthCubemap, direction);

    if(enableEVSM == 1) {
        vec4 pm1s = exp(cPos * m1s);
        vec4 nm1s = -exp(-cNeg * m1s);
        vec4 pm2s = pm1s*pm1s;
        vec4 nm2s = nm1s*nm1s;

        moments = vec4(pm1s.x, pm2s.x, nm1s.x, nm2s.x)
            + vec4(pm1s.y, pm2s.y, nm1s.y, nm2s.y)
            + vec4(pm1s.z, pm2s.z, nm1s.z, nm2s.z)
            + vec4(pm1s.w, pm2s.w, nm1s.w, nm2s.w);
        moments *= 0.25;

        vec4 m = vec4(exp(cPos * depth), 0, -exp(-cNeg * depth), 0);
        m.yw = m.xz * m.xz;

       // moments = m;
        moments = mix(moments, m, 0.25);

        //moments = m;

        //moments = vec4(depth * 0.5 * direction + 0.5, 1.0);

    } else {
        //vec4 m2s = m1s*m1s;

        //moments = vec4(vec2(m1s.x, m2s.x) + vec2(m1s.y, m2s.y) + vec2(m1s.z, m2s.z) + vec2(m1s.w, m2s.w), 0, 1);
        //moments *= 0.25;

        moments = vec4(depth, depth*depth, 0, 1);
    }

    //moments = vec4(1, 1, 1, 1);
}
