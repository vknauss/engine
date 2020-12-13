#ifndef BOUNDING_SPHERE_H_INCLUDED
#define BOUNDING_SPHERE_H_INCLUDED

#include <glm/glm.hpp>


class BoundingSphere {

    glm::vec3 m_position;

    float m_radius;

public:

    BoundingSphere() :
        m_position(0.0f),
        m_radius(0.0f) {
    }

    BoundingSphere(const glm::vec3& position, float radius) :
        m_position(position),
        m_radius(radius) {
    }

    BoundingSphere(const BoundingSphere& b) :
        BoundingSphere(b.m_position, b.m_radius) {
    }

    BoundingSphere(BoundingSphere&& b) :
        m_position(std::move(b.m_position)),
        m_radius(std::move(b.m_radius)) {
    }

    BoundingSphere& operator=(const BoundingSphere& b) {
        m_position = b.m_position;
        m_radius = b.m_radius;
        return *this;
    }

    BoundingSphere& operator=(BoundingSphere&& b) {
        m_position = std::move(b.m_position);
        m_radius = std::move(b.m_radius);
        return *this;
    }

    BoundingSphere& setPosition(const glm::vec3& position) {
        m_position = position;
        return *this;
    }

    BoundingSphere& setRadius(float radius) {
        m_radius = radius;
        return *this;
    }

    glm::vec3 getPosition() const {
        return m_position;
    }

    float getRadius() const {
        return m_radius;
    }

    BoundingSphere& add(const BoundingSphere& b) {
        float d = glm::length(m_position - b.m_position);
        bool is1 = (m_radius >= b.m_radius);
        float r1 = is1 ? m_radius : b.m_radius;
        float r2 = is1 ? b.m_radius : m_radius;

        if (r1 >= d + r2) {
            if (!is1) {
                *this = b;
            }
        } else {
            float r = 0.5f * (r1 + r2 + d);
            glm::vec3 v = b.m_position - m_position;
            v /= d;
            m_position = m_position + (r - m_radius) * v;
            m_radius = r;
        }

        return *this;
    }
};

#endif // BOUNDING_SPHERE_H_INCLUDED
