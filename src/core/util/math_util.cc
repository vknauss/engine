#include "math_util.h"

#include <numeric>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>

//#include <iostream>
//#include <glm/gtx/string_cast.hpp>

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
int triangleClip(const glm::vec3* pointsIn, float* boundsAA, glm::vec3* pointsOut) {
    // I believe a triangle clipped against a rectangle may have no more than 7 points
    // Could be wrong for a while I thought 6, but rarely 7 is possible with a big narrow triangle
    glm::vec3 points[7] = {pointsIn[0], pointsIn[1], pointsIn[2]};
    bool clip[7];
    int nPoints = 3;
    // For each rectangle edge
    for (int i = 0; i < 4; i++) {
        bool x = (i/2 == 0);
        bool l = (i%2 == 0);

        // For each point, compute if it is on the inside or outside of this
        // edge, and store the result in clip
        for (int pi = 0; pi < nPoints; ++pi) {
            if (x) {
                if (l) {
                    clip[pi] = points[pi].x < boundsAA[i];
                } else {
                    clip[pi] = points[pi].x > boundsAA[i];
                }
            } else {
                if (l) {
                    clip[pi] = points[pi].y < boundsAA[i];
                } else {
                    clip[pi] = points[pi].y > boundsAA[i];
                }
            }
        }

        int nnPoints = 0;
        for (int pi = 0; pi < nPoints; ++pi) {
            int pp = (pi == 0) ? nPoints - 1 : pi - 1;

            float t = x ?
                (boundsAA[i] - points[pp].x) / (points[pi].x - points[pp].x) :
                (boundsAA[i] - points[pp].y) / (points[pi].y - points[pp].y);

            if (!clip[pi]) {
                if (clip[pp]) {
                    pointsOut[nnPoints++] = points[pp] + t * (points[pi] - points[pp]);
                }
                pointsOut[nnPoints++] = points[pi];
            } else {
                if(!clip[pp]) {
                    pointsOut[nnPoints++] = points[pp] + t * (points[pi] - points[pp]);
                }
            }
        }
        std::copy(pointsOut, pointsOut+nnPoints, points);
        nPoints = nnPoints;
        assert(nPoints <= 7);
    }
    return nPoints;
}

void getClippedNearPlane(float left, float right, float bottom, float top, const glm::vec3* viewSpaceBBVerticesIn, float* nearOut) {
    glm::vec3 points[8];
    std::copy(viewSpaceBBVerticesIn, viewSpaceBBVerticesIn+8, points);
    glm::ivec3 triangles[] = {
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
        glm::vec3 triPointsClipped[7];
        int nClippedPoints = triangleClip(triPoints, bounds, triPointsClipped);
        clippedPoints.insert(clippedPoints.end(), triPointsClipped, triPointsClipped+nClippedPoints);
    }

    for (const glm::vec3& p : clippedPoints) {
        if (-p.z < (*nearOut)) (*nearOut) = -p.z;
    }
}


std::array<Plane, 6> frustumPlanes(const glm::mat4& frustumMatrix) {
    std::array<Plane, 6> planes;
    glm::mat4 m = glm::transpose(frustumMatrix);
    for(int i = 0; i < 6; ++i) {
        planes[i] = Plane(((i%2)*2.0f-1.0f) * m[i/2] + m[3]);
    }
    return planes;
}


void frustumCullSpheres(glm::mat4 frustumMatrix, int nSpheresIn, const glm::vec4* spheresIn, int* cullResultsOut, int* numPassed) {
    std::array<Plane, 6> planes = frustumPlanes(frustumMatrix);

    (*numPassed) = nSpheresIn;

    for(int i = 0; i < nSpheresIn; ++i) {
        glm::vec3 center = glm::vec3(spheresIn[i]);
        float radius = spheresIn[i][3];

        cullResultsOut[i] = 1;
        for(const Plane& p : planes) {
            if(glm::dot(p.normal, center) + p.offset < -radius) {
                cullResultsOut[i] = 0;
                (*numPassed) --;
                break;
            }
        }
    }
}

void frustumCullSpheres(glm::mat4 frustumMatrix, int nSpheresIn, const glm::vec4* spheresIn, std::vector<bool>& cullResultsOut, int* numPassed) {
    std::array<Plane, 6> planes = frustumPlanes(frustumMatrix);

    (*numPassed) = nSpheresIn;

    for(int i = 0; i < nSpheresIn; ++i) {
        glm::vec3 center = glm::vec3(spheresIn[i]);
        float radius = spheresIn[i][3];

        cullResultsOut[i] = true;
        for(const Plane& p : planes) {
            if(glm::dot(p.normal, center) + p.offset < -radius) {
                cullResultsOut[i] = false;
                (*numPassed) --;
                break;
            }
        }
    }
}

void frustumCullSpheres(glm::mat4 frustumMatrix, int nSpheresIn, const glm::vec4* spheresIn, glm::vec4* cullResultsOut, int* numPassed) {
    std::array<Plane, 6> planes = frustumPlanes(frustumMatrix);

    (*numPassed) = 0;

    for(int i = 0; i < nSpheresIn; ++i) {
        glm::vec3 center = glm::vec3(spheresIn[i]);
        float radius = spheresIn[i][3];

        bool cull = false;
        for(const Plane& p : planes) {
            if(glm::dot(p.normal, center) + p.offset < -radius) {
                cull = true;
                break;
            }
        }
        if(cull) continue;

        cullResultsOut[(*numPassed)++] = spheresIn[i];
    }
}

// Build Delaunay triangulation of 2D positions
// Bowyer-Watson Algorithm
// https://en.wikipedia.org/wiki/Bowyer%E2%80%93Watson_algorithm
// Based on explanation and pseudo-code from https://towardsdatascience.com/delaunay-triangulation-228a86d1ddad
// Circumcircle determination matrix method from https://en.wikipedia.org/wiki/Delaunay_triangulation#Algorithms
std::vector<glm::uvec3> delaunayTriangulation(const std::vector<glm::vec2>& positions) {
    // Find enclosing triangle of all points
    glm::vec2 pmax(std::numeric_limits<float>::min());
    glm::vec2 pmin(std::numeric_limits<float>::max());
    pmax = std::accumulate(positions.begin(), positions.end(), pmax, [] (const glm::vec2& p0, const glm::vec2& p1) { return glm::max(p0, p1); });
    pmin = std::accumulate(positions.begin(), positions.end(), pmin, [] (const glm::vec2& p0, const glm::vec2& p1) { return glm::min(p0, p1); });

    // Vertices of an isosceles triangle guaranteed to fully enclose the points, with no point in the set on the border
    // See https://towardsdatascience.com/delaunay-triangulation-228a86d1ddad for illustration.
    float bheight = pmax.y - pmin.y, bwidth = pmax.x - pmin.x;
    glm::vec2 ptop = {0.5f * (pmax.x + pmin.x), pmax.y + bheight};
    glm::vec2 pleft = {pmin.x - 0.5f * bwidth - 0.1f * bheight, pmin.y - 0.1f * 0.5f * bwidth};  // 0.1f * [0.5f * bwidth || bheight] is a buffer to prevent points from being on the bottom edge or the corners
    glm::vec2 pright = {pmax.x + 0.5f * bwidth + 0.1f * bheight, pmin.y - 0.1f * 0.5f * bwidth};

    // Initialize the algorithm
    std::vector<glm::uvec3> tris;
    std::vector<glm::vec2> points(positions);

    tris.emplace_back(points.size(), points.size()+1, points.size()+2);
    points.insert(points.end(), {ptop, pleft, pright});  // note: CCW

    // Lambda for convenience
    auto circumcircle = [] (const glm::vec2& a, const glm::vec2& b, const glm::vec2& c, const glm::vec2& d) {
        glm::mat4 m(a.x, b.x, c.x, d.x,
                    a.y, b.y, c.y, d.y,
                    glm::dot(a, a), glm::dot(b, b), glm::dot(c, c), glm::dot(d, d),
                    1, 1, 1, 1);
        return glm::determinant(m) > 0.0f;
    };

    // Edge matrix used to store the edges of the polygon formed by the to-be-removed triangles
    //   constant size so just allocate once
    // Values: 0: not yet considered (not in polygon), 1: in polygon, 2: removed from polygon
    std::vector<uint8_t> polyEdges(points.size()*points.size(), 0u);

    // Flags to mark whether a triangle will contribute to the final triangulation
    std::vector<bool> reject(tris.size(), false);

    // Number of triangles in the final triangulation
    // Maintained to match the number of false entries in reject
    size_t nTris = 1;

    // Build triangulation
    for (size_t i = 0; i < points.size() - 3; ++i) {  // -3 since we don't care about the bounding triangle's vertices
        glm::vec2 p = points[i];

        // Find which triangles the given point is within the circumcircle of
        std::vector<size_t> bad;
        for (size_t j = 0; j < tris.size(); ++j) {
            if (reject[j]) continue;
            if (circumcircle(points[tris[j][0]], points[tris[j][1]], points[tris[j][2]], p)) bad.push_back(i);
        }

        // Find the polygonal region formed by these bad triangles and remove from triangulation
        polyEdges.assign(polyEdges.size(), 0u);
        for (size_t j : bad) {
            reject[j] = true;
            --nTris;
            for (auto k = 0; k < 3; ++k) {
                auto i0 = tris[j][k];
                auto i1 = tris[j][(k+1)%3];
                auto ind = std::min(i0, i1) * points.size() + std::max(i0, i1);  // edges are indexed to be order-independent (half of the matrix is wasted space)
                if (polyEdges[ind] == 0u) {
                    polyEdges[ind] = 1u;
                } else {
                    polyEdges[ind] = 2u;
                }
            }
        }

        // Re-triangulate polygon area
        for (size_t i0 = 0; i0 < points.size(); ++i0) {
            for (size_t i1 = i0 + 1; i1 < points.size(); ++i1) {
                // Find CCW order
                glm::vec3 e03 = glm::vec3(points[i1] - points[i0], 0.0f);
                glm::vec3 e13 = glm::vec3(p - points[i1], 0.0f);
                if (glm::cross(e03, e13).z > 0.0f) {
                    tris.emplace_back(i0, i1, i);
                } else {
                    tris.emplace_back(i, i1, i0);
                }
                reject.push_back(false);
                ++nTris;
            }
        }
    }

    // Remove triangles containing bounding-triangle vertices
    for (size_t i = 0; i < tris.size(); ++i) {
        if (reject[i]) continue;
        if (tris[i][0] >= points.size()-3 || tris[i][1] >= points.size()-3 || tris[i][2] >= points.size()-3) {
            reject[i] = true;
            --nTris;
        }
    }

    // Build final triangle list
    std::vector<glm::uvec3> finalTris(nTris);
    size_t cInd = 0;
    for (size_t i = 0; i < tris.size() && cInd < nTris; ++i) {
        if (!reject[i]) finalTris[cInd++] = tris[i];
    }

    return finalTris;
}

}  // namespace math_util
