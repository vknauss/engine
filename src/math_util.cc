#include "math_util.h"

#include <vector>

#include <glm/gtc/matrix_inverse.hpp>

#include <iostream>
#include <glm/gtx/string_cast.hpp>

namespace math_util {

void getWorldSpaceFrustumVertices(const glm::mat4& frustumMatrix, glm::vec3* verticesOut) {
    glm::mat4 invFM = glm::inverse(frustumMatrix);

    glm::vec4 fsVecs[] = {
        {-1, -1, -1, 1}, {1, -1, -1, 1}, {-1, 1, -1, 1}, {1, 1, -1, 1},
        {-1, -1,  1, 1}, {1, -1,  1, 1}, {-1, 1,  1, 1}, {1, 1,  1, 1}
    };

    for(int i = 0; i < 8; ++i){
        glm::vec4 ws = invFM * fsVecs[i];
        verticesOut[i] = glm::vec3(ws) / ws.w;
    }
}

void getWorldSpaceFrustumVertices(const glm::mat4& frustumMatrix,
    glm::vec3& nearBottomLeft, glm::vec3& nearBottomRight, glm::vec3& nearTopLeft, glm::vec3& nearTopRight,
    glm::vec3&  farBottomLeft, glm::vec3&  farBottomRight, glm::vec3&  farTopLeft, glm::vec3&  farTopRight)
{
    glm::vec3 wsVecs[8];
    getWorldSpaceFrustumVertices(frustumMatrix, wsVecs);

    nearBottomLeft = wsVecs[0];
    nearBottomRight = wsVecs[1];
    nearTopLeft = wsVecs[2];
    nearTopRight = wsVecs[3];
    farBottomLeft = wsVecs[4];
    farBottomRight = wsVecs[5];
    farTopLeft = wsVecs[6];
    farTopRight = wsVecs[7];
}

void getAlignedBoundingBoxExtents(const glm::mat4& basis, int nVerticesIn, const glm::vec3* verticesIn, glm::vec3& minExtents, glm::vec3& maxExtents) {
    std::vector<glm::vec3> lsVerts(nVerticesIn);
    for(int i = 0; i < nVerticesIn; ++i) {
        glm::vec4 ws = glm::vec4(verticesIn[i], 1.0);
        lsVerts[i] = glm::vec3(basis * ws);
    }

    minExtents = lsVerts[0];
    maxExtents = lsVerts[0];
    for(size_t i = 1; i < lsVerts.size(); ++i) {
        minExtents = glm::min(minExtents, lsVerts[i]);
        maxExtents = glm::max(maxExtents, lsVerts[i]);
    }
}

void getAlignedBoundingBoxVertices(const glm::mat4& basis, int nVerticesIn, const glm::vec3* verticesIn, glm::vec3* verticesOut) {
    glm::vec3 bounds[2];

    getAlignedBoundingBoxExtents(basis, nVerticesIn, verticesIn, bounds[0], bounds[1]);

    glm::mat4 iBasis = glm::inverse(basis);
    for(size_t i = 0; i < 8; ++i) {
        glm::vec4 ls = glm::vec4(bounds[i%2==1][0], bounds[(i/2)%2==1][1], bounds[i/4==1][2], 1);
        verticesOut[i] = glm::vec3(iBasis * ls);
    }

}

void getAlignedBoundingBoxVerticesLocal(const glm::mat4& basis, int nVerticesIn, const glm::vec3* verticesIn, glm::vec3* verticesOut) {
    glm::vec3 bounds[2];

    getAlignedBoundingBoxExtents(basis, nVerticesIn, verticesIn, bounds[0], bounds[1]);

    for(size_t i = 0; i < 8; ++i) {
        verticesOut[i] = glm::vec3(bounds[i%2==1][0], bounds[(i/2)%2==1][1], bounds[i/4==1][2]);
    }
}



// Sutherland-Hodgman algorithm
// based on pseudocode from
// https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm
std::vector<glm::vec3> triangleClip(const glm::vec3* pointsIn, float* boundsAA) {
    std::vector<glm::vec3> points = {pointsIn[0], pointsIn[1], pointsIn[2]};
    for(int i = 0; i < 4; i++) {
        bool x = (i/2 == 0);
        bool l = (i%2 == 0);

        std::vector<glm::vec3> pointsOut;
        std::vector<bool> clip(points.size(), false);
        for(size_t pi = 0; pi < points.size(); ++pi) {
            bool c = false;
            if(x) {
                if(l) {
                    c = points[pi].x < boundsAA[i];
                } else {
                    c = points[pi].x > boundsAA[i];
                }
            } else {
                if(l) {
                    c = points[pi].y < boundsAA[i];
                } else {
                    c = points[pi].y > boundsAA[i];
                }
            }
            if(c) clip[pi] = true;
        }
        for(size_t pi = 0; pi < points.size(); ++pi) {
            size_t pp = (pi == 0) ? points.size() - 1 : pi - 1;

            float t;
            if(x) {
                t = (boundsAA[i] - points[pp].x) / (points[pi].x - points[pp].x);
            } else {
                t = (boundsAA[i] - points[pp].y) / (points[pi].y - points[pp].y);
            }

            if(!clip[pi]) {
                if(clip[pp]) {
                    pointsOut.push_back(points[pp] + t * (points[pi] - points[pp]));
                }
                pointsOut.push_back(points[pi]);
            } else {
                if(!clip[pp]) {
                    pointsOut.push_back(points[pp] + t * (points[pi] - points[pp]));
                }
            }
        }
        points = pointsOut;
    }
    return points;
}

void getClippedNearPlane(float left, float right, float bottom, float top, const glm::vec3* viewSpaceBBVerticesIn, float* nearOut) {
    std::vector<glm::vec3> points(viewSpaceBBVerticesIn, viewSpaceBBVerticesIn+8);
    std::vector<glm::ivec3> triangles = {
        {0, 1, 3}, {0, 3, 2},  // Front
        {5, 4, 6}, {5, 6, 7},  // Back
        {1, 5, 7}, {1, 7, 3},  // Right
        {4, 0, 2}, {4, 2, 6},  // Left
        {2, 3, 7}, {2, 7, 6},  // Top
        {4, 5, 1}, {4, 1, 0}   // Bottom
    };

    std::vector<glm::vec3> clippedPoints;

    float bounds[] = {left, right, bottom, top};
    for(const glm::ivec3& tri : triangles) {
        glm::vec3 triPoints[] = {points[tri[0]], points[tri[1]], points[tri[2]]};
        std::vector<glm::vec3> triPointsClipped = triangleClip(triPoints, bounds);
        clippedPoints.insert(clippedPoints.end(), triPointsClipped.begin(), triPointsClipped.end());
    }

    for(const glm::vec3& p : clippedPoints) {
        if(-p.z < (*nearOut)) (*nearOut) = -p.z;
    }
}

struct Plane {
    glm::vec3 normal;
    float offset;
};

void setPlane(Plane& p, const glm::vec3& n, float o) {
    float l = glm::length(n);
    p.normal = n / l;
    p.offset = o / l;
}

void setPlane(Plane& p, const glm::vec4& abcd) {
    setPlane(p, glm::vec3(abcd), abcd[3]);
}

void frustumCullSpheres(glm::mat4 frustumMatrix, int nSpheresIn, const glm::vec4* spheresIn, int* cullResultsOut, int* numPassed) {
    frustumMatrix = glm::transpose(frustumMatrix);

    Plane frustumPlanes[6];
    for(int i = 0; i < 6; ++i) {
        setPlane(frustumPlanes[i], ((i%2)*2.0f-1.0f) * frustumMatrix[i/2] + frustumMatrix[3]);
    }

    (*numPassed) = nSpheresIn;

    for(int i = 0; i < nSpheresIn; ++i) {
        glm::vec3 center = glm::vec3(spheresIn[i]);
        float radius = spheresIn[i][3];

        cullResultsOut[i] = 1;
        for(const Plane& p : frustumPlanes) {
            if(glm::dot(p.normal, center) + p.offset < -radius) {
                cullResultsOut[i] = 0;
                (*numPassed) --;
                break;
            }
        }
    }
}

void frustumCullSpheres(glm::mat4 frustumMatrix, int nSpheresIn, const glm::vec4* spheresIn, std::vector<bool>& cullResultsOut, int* numPassed) {
    frustumMatrix = glm::transpose(frustumMatrix);

    Plane frustumPlanes[6];
    for(int i = 0; i < 6; ++i) {
        setPlane(frustumPlanes[i], ((i%2)*2.0f-1.0f) * frustumMatrix[i/2] + frustumMatrix[3]);
    }

    (*numPassed) = nSpheresIn;

    for(int i = 0; i < nSpheresIn; ++i) {
        glm::vec3 center = glm::vec3(spheresIn[i]);
        float radius = spheresIn[i][3];

        cullResultsOut[i] = true;
        for(const Plane& p : frustumPlanes) {
            if(glm::dot(p.normal, center) + p.offset < -radius) {
                cullResultsOut[i] = false;
                (*numPassed) --;
                break;
            }
        }
    }
}

void frustumCullSpheres(glm::mat4 frustumMatrix, int nSpheresIn, const glm::vec4* spheresIn, glm::vec4* cullResultsOut, int* numPassed) {
    frustumMatrix = glm::transpose(frustumMatrix);

    Plane frustumPlanes[6];
    for(int i = 0; i < 6; ++i) {
        setPlane(frustumPlanes[i], ((i%2)*2.0f-1.0f) * frustumMatrix[i/2] + frustumMatrix[3]);
    }

    (*numPassed) = 0;

    for(int i = 0; i < nSpheresIn; ++i) {
        glm::vec3 center = glm::vec3(spheresIn[i]);
        float radius = spheresIn[i][3];

        bool cull = false;
        for(const Plane& p : frustumPlanes) {
            if(glm::dot(p.normal, center) + p.offset < -radius) {
                cull = true;
                break;
            }
        }
        if(cull) continue;

        cullResultsOut[(*numPassed)++] = spheresIn[i];
    }
}

}  // namespace math_util
