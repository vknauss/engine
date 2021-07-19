#ifndef BOUNDING_SPHERE_H_INCLUDED
#define BOUNDING_SPHERE_H_INCLUDED

#include <glm/glm.hpp>


struct BoundingSphere {

    glm::vec3 position = glm::vec3(0);
    float radius = 0.0f;

    BoundingSphere& add(const BoundingSphere& b) {
        float d = glm::length(position - b.position);
        bool is1 = (radius >= b.radius);
        float r1 = is1 ? radius : b.radius;
        float r2 = is1 ? b.radius : radius;

        if (r1 >= d + r2) {
            if (!is1) {
                *this = b;
            }
        } else {
            float r = 0.5f * (r1 + r2 + d);
            glm::vec3 v = b.position - position;
            v /= d;
            position = position + (r - radius) * v;
            radius = r;
        }

        return *this;
    }
};

#endif // BOUNDING_SPHERE_H_INCLUDED
