#ifndef MATH_UTIL_H_
#define MATH_UTIL_H_

#include <vector>

#include <glm/glm.hpp>

namespace math_util {

void getWorldSpaceFrustumVertices(const glm::mat4& frustumMatrix, glm::vec3* verticesOut);

void getWorldSpaceFrustumVertices(const glm::mat4& frustumMatrix,
    glm::vec3& nearBottomLeft, glm::vec3& nearBottomRight, glm::vec3& nearTopLeft, glm::vec3& nearTopRight,
    glm::vec3&  farBottomLeft, glm::vec3&  farBottomRight, glm::vec3&  farTopLeft, glm::vec3&  farTopRight);

void getAlignedBoundingBoxExtents(const glm::mat4& basis, int nVerticesIn, const glm::vec3* verticesIn, glm::vec3& minExtents, glm::vec3& maxExtents);

void getAlignedBoundingBoxVertices(const glm::mat4& basis, int nVerticesIn, const glm::vec3* verticesIn, glm::vec3* verticesOut);

void getClippedNearPlane(float left, float right, float bottom, float top, const glm::vec3* viewSpaceBBVerticesIn, float* nearOut);

void frustumCullSpheres(glm::mat4 frustumMatrix, int nSpheresIn, const glm::vec4* spheresIn, int* cullResultsOut, int* numPassed);
void frustumCullSpheres(glm::mat4 frustumMatrix, int nSpheresIn, const glm::vec4* spheresIn, std::vector<bool>& cullResultsOut, int* numPassed);
void frustumCullSpheres(glm::mat4 frustumMatrix, int nSpheresIn, const glm::vec4* spheresIn, glm::vec4* cullResultsOut, int* numPassed);

}  // namespace math_util

#endif // MATH_UTIL_H_
