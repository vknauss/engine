#include "mesh_builder.h"

#include <algorithm>
#include <numeric>

#include <glm/gtc/matrix_inverse.hpp>

MeshBuilder::MeshBuilder() :
    m_meshData() {
}

MeshBuilder::MeshBuilder(const MeshData& md) :
    m_meshData(md) {
}

MeshBuilder::MeshBuilder(MeshData&& md) :
    m_meshData(md) {
}

void MeshBuilder::moveMeshData(MeshData& md) {
    md = std::move(m_meshData);
}

MeshData&& MeshBuilder::moveMeshData() {
    return std::move(m_meshData);
}

MeshBuilder& MeshBuilder::add(const MeshData& m) {
    if (m_meshData.hasNormals() && m.hasNormals()) {
        m_meshData.normals.insert(m_meshData.normals.end(), m.normals.begin(), m.normals.end());
    }

    if (m_meshData.hasTangents() && m.hasTangents()) {
        m_meshData.tangents.insert(m_meshData.tangents.end(), m.tangents.begin(), m.tangents.end());
    }

    if (m_meshData.hasBitangents() && m.hasBitangents()) {
        m_meshData.bitangents.insert(m_meshData.bitangents.end(), m.bitangents.begin(), m.bitangents.end());
    }

    if (m_meshData.hasUVs() && m.hasUVs()) {
        m_meshData.uvs.insert(m_meshData.uvs.end(), m.uvs.begin(), m.uvs.end());
    }

    if (m_meshData.hasBoneIndices() && m.hasBoneIndices()) {
        m_meshData.boneIndices.insert(m_meshData.boneIndices.end(), m.boneIndices.begin(), m.boneIndices.end());
    }

    if (m_meshData.hasBoneWeights() && m.hasBoneWeights()) {
        m_meshData.boneWeights.insert(m_meshData.boneWeights.end(), m.boneWeights.begin(), m.boneWeights.end());
    }

    size_t numIndices = m_meshData.indices.size();
    m_meshData.indices.resize(numIndices + m.indices.size());

    auto offsetIndex = [offset = m_meshData.vertices.size()] (uint32_t index) { return index + offset; };

    std::transform(m.indices.begin(), m.indices.end(), m_meshData.indices.begin() + numIndices, offsetIndex);

    m_meshData.vertices.insert(m_meshData.vertices.end(), m.vertices.begin(), m.vertices.end());

    return *this;
}

MeshBuilder& MeshBuilder::calculateNormals() {
    m_meshData.normals.assign(m_meshData.getNumVertices(), glm::vec3(0.0f));

    for (size_t i = 0; i < m_meshData.indices.size()/3; ++i) {
        uint32_t
            i0 = m_meshData.indices[3*i],
            i1 = m_meshData.indices[3*i+1],
            i2 = m_meshData.indices[3*i+2];

        glm::vec3 n = glm::normalize(glm::cross(
            m_meshData.vertices[i1] - m_meshData.vertices[i0],
            m_meshData.vertices[i2] - m_meshData.vertices[i0]));

        m_meshData.normals[i0] += n;
        m_meshData.normals[i1] += n;
        m_meshData.normals[i2] += n;
    }

    std::transform(m_meshData.normals.begin(), m_meshData.normals.end(),
                   m_meshData.normals.begin(),
                   [] (const glm::vec3& v) { return glm::normalize(v); });

    return *this;
}

MeshBuilder& MeshBuilder::calculateTangentSpace() {
    if (!m_meshData.hasUVs()) {
        return *this;
    }
    if (!m_meshData.hasNormals()) {
        calculateNormals();
    }

    m_meshData.tangents.assign(m_meshData.vertices.size(), glm::vec3(0.0));

    for(size_t i = 0; i < m_meshData.indices.size()/3; ++i) {
        uint32_t
            i0 = m_meshData.indices[3*i],
            i1 = m_meshData.indices[3*i+1],
            i2 = m_meshData.indices[3*i+2];

        glm::vec3
            v0 = m_meshData.vertices[i0],
            v1 = m_meshData.vertices[i1],
            v2 = m_meshData.vertices[i2];

        glm::vec2
            t0 = m_meshData.uvs[i0],
            t1 = m_meshData.uvs[i1],
            t2 = m_meshData.uvs[i2];

        glm::vec2
            e1t = t1 - t0,
            e2t = t2 - t0;

        glm::mat2x3 m1(v1-v0, v2-v0);

        glm::mat2 m2 = (1.0f/(e1t.x*e2t.y - e2t.x*e1t.y)) * glm::mat2(e2t.y, -e1t.y, -e2t.x, e1t.x);

        glm::mat2x3 tb = m1 * m2;

        glm::vec3 t = tb[0];

        m_meshData.tangents[i0] += t;
        m_meshData.tangents[i1] += t;
        m_meshData.tangents[i2] += t;
    }

    m_meshData.bitangents.resize(m_meshData.vertices.size());

    for(size_t i = 0; i < m_meshData.vertices.size(); ++i) {
        glm::vec3
            t = m_meshData.tangents[i],
            n = m_meshData.normals[i];

        m_meshData.tangents[i] = glm::normalize(t - n * dot(t, n));
        m_meshData.bitangents[i] = glm::cross(n, m_meshData.tangents[i]);
    }

    return *this;
}

MeshBuilder& MeshBuilder::translate(const glm::vec3& t) {
    std::transform(m_meshData.vertices.begin(), m_meshData.vertices.end(),
                   m_meshData.vertices.begin(),
                   [t=t] (const glm::vec3& v) { return v + t; });
    return *this;
}

MeshBuilder& MeshBuilder::rotate(const glm::quat& q) {
    auto qrotate = [&, qi=glm::inverse(q)] (const glm::vec3& v) { return q*v*qi; };

    std::transform(m_meshData.vertices.begin(), m_meshData.vertices.end(),
                   m_meshData.vertices.begin(), qrotate);

    if (m_meshData.hasNormals()) {
        std::transform(m_meshData.normals.begin(), m_meshData.normals.end(),
                       m_meshData.normals.begin(), qrotate);
    }

    if (m_meshData.hasTangents()) {
        std::transform(m_meshData.tangents.begin(), m_meshData.tangents.end(),
                       m_meshData.tangents.begin(), qrotate);
    }

    if (m_meshData.hasBitangents()) {
        std::transform(m_meshData.bitangents.begin(), m_meshData.bitangents.end(),
                       m_meshData.bitangents.begin(), qrotate);
    }

    return *this;
}

MeshBuilder& MeshBuilder::rotate(const glm::mat3& m) {
    auto mrotate = [&] (const glm::vec3& v) { return m*v; };

    std::transform(m_meshData.vertices.begin(), m_meshData.vertices.end(),
                   m_meshData.vertices.begin(), mrotate);

    if (m_meshData.hasNormals()) {
        std::transform(m_meshData.normals.begin(), m_meshData.normals.end(),
                       m_meshData.normals.begin(), mrotate);
    }

    if (m_meshData.hasTangents()) {
        std::transform(m_meshData.tangents.begin(), m_meshData.tangents.end(),
                       m_meshData.tangents.begin(), mrotate);
    }

    if (m_meshData.hasBitangents()) {
        std::transform(m_meshData.bitangents.begin(), m_meshData.bitangents.end(),
                       m_meshData.bitangents.begin(), mrotate);
    }

    return *this;
}

MeshBuilder& MeshBuilder::scale(const glm::vec3& v) {
    std::transform(m_meshData.vertices.begin(), m_meshData.vertices.end(),
                   m_meshData.vertices.begin(),
                   [s=v] (const glm::vec3& v) { return s*v; });

    if (m_meshData.hasNormals()) {
        std::transform(m_meshData.normals.begin(), m_meshData.normals.end(),
                       m_meshData.normals.begin(),
                       [s=1.0f/v] (const glm::vec3& v) { return glm::normalize(s*v); });
    }

    if (m_meshData.hasTangents()) {
        std::transform(m_meshData.tangents.begin(), m_meshData.tangents.end(),
                       m_meshData.tangents.begin(),
                       [s=v] (const glm::vec3& v) { return glm::normalize(s*v); });
    }

    // Really only need to check hasBitangents(), however just to be explicit
    if (m_meshData.hasNormals() && m_meshData.hasTangents() && m_meshData.hasBitangents()) {
        std::transform(m_meshData.normals.begin(), m_meshData.normals.end(), m_meshData.tangents.begin(),
                       m_meshData.bitangents.begin(),
                       [] (const glm::vec3& n, const glm::vec3& t) { return glm::cross(n, t); });
    }

    return *this;
}

MeshBuilder& MeshBuilder::scale(float s) {
    std::transform(m_meshData.vertices.begin(), m_meshData.vertices.end(),
                   m_meshData.vertices.begin(),
                   [s=s] (const glm::vec3& v) { return s*v; });

    return *this;
}

MeshBuilder& MeshBuilder::transform(const glm::mat4& m) {
    std::transform(m_meshData.vertices.begin(), m_meshData.vertices.end(),
                   m_meshData.vertices.begin(),
                   [m=m] (const glm::vec3& v) { return glm::vec3(m * glm::vec4(v, 1.0f)); });

    if (m_meshData.hasNormals()) {
        std::transform(m_meshData.normals.begin(), m_meshData.normals.end(),
                       m_meshData.normals.begin(),
                       [m=glm::inverseTranspose(glm::mat3(m))] (const glm::vec3& v) { return glm::normalize(m*v); });
    }

    if (m_meshData.hasTangents()) {
        std::transform(m_meshData.tangents.begin(), m_meshData.tangents.end(),
                       m_meshData.tangents.begin(),
                       [m=glm::mat3(m)] (const glm::vec3& v) { return glm::normalize(m*v); });
    }

    // Really only need to check hasBitangents(), however just to be explicit
    if (m_meshData.hasNormals() && m_meshData.hasTangents() && m_meshData.hasBitangents()) {
        std::transform(m_meshData.normals.begin(), m_meshData.normals.end(), m_meshData.tangents.begin(),
                       m_meshData.bitangents.begin(),
                       [] (const glm::vec3& n, const glm::vec3& t) { return glm::cross(n, t); });
    }

    return *this;
}

MeshBuilder& MeshBuilder::cylinder(float radius, float height, int circlePoints, int segments, bool makeClosed) {
    if (circlePoints < 3 || segments < 1) {
        return *this;
    }

    size_t nVertices = static_cast<size_t> ((segments+1) * circlePoints + (makeClosed ? 2 * circlePoints : 0));
    size_t c_vertexIndex = m_meshData.vertices.size();
    size_t c_index = m_meshData.indices.size();
    size_t indexOffset = c_vertexIndex;

    m_meshData.vertices.resize(c_vertexIndex + nVertices);
    m_meshData.normals.resize(m_meshData.vertices.size());
    m_meshData.indices.resize(c_index + 6*(segments*circlePoints + (makeClosed ? circlePoints-2 : 0)));

    float c_angle = (float) M_PI * 2.0f / circlePoints;
    for (int i = 0; i < segments+1; ++i) {
        for (int j = 0; j < circlePoints; ++j) {
            if (i == 0) {
                glm::vec3 n = glm::vec3(glm::sin(j*c_angle), 0, glm::cos(j*c_angle));
                m_meshData.vertices[c_vertexIndex] = radius*n + glm::vec3(0, -height/2.0f, 0);
                m_meshData.normals[c_vertexIndex] = n;
            } else {
                m_meshData.vertices[c_vertexIndex] = m_meshData.vertices[c_vertexIndex - circlePoints] + glm::vec3(0, height/segments, 0);
                m_meshData.normals[c_vertexIndex] = m_meshData.normals[c_vertexIndex - circlePoints];
            }
            ++c_vertexIndex;

            if (i > 0) {
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + j);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + (j+1)%circlePoints);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + j);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + j);
            }
        }
    }

    if (makeClosed) {
        for (int i = 0; i < 2; ++i) {
            uint32_t baseIndex = c_vertexIndex;
            for (int j = 0; j < circlePoints; ++j) {
                if (i == 0) {
                    m_meshData.vertices[c_vertexIndex] = m_meshData.vertices[indexOffset + circlePoints - j - 1];
                    m_meshData.normals[c_vertexIndex] = glm::vec3(0, -1, 0);
                } else {
                    m_meshData.vertices[c_vertexIndex] = m_meshData.vertices[indexOffset + segments * circlePoints + j];
                    m_meshData.normals[c_vertexIndex] = glm::vec3(0, 1, 0);
                }

                if (j > 1) {
                    m_meshData.indices[c_index++] = c_vertexIndex;
                    m_meshData.indices[c_index++] = baseIndex;
                    m_meshData.indices[c_index++] = c_vertexIndex-1;
                }

                ++c_vertexIndex;
            }
        }
    }

    return *this;
}

MeshBuilder& MeshBuilder::hemisphere(float radius, int circlePoints, int rings, bool makeClosed) {
    if (circlePoints < 3 || rings < 1) {
        return *this;
    }

    size_t nVertices = static_cast<size_t> (rings * circlePoints + 1 + (makeClosed ? circlePoints : 0));
    size_t c_vertexIndex = m_meshData.vertices.size();
    size_t c_index = m_meshData.indices.size();
    size_t indexOffset = c_vertexIndex;

    m_meshData.vertices.resize(c_vertexIndex + nVertices);
    m_meshData.normals.resize(m_meshData.vertices.size());
    m_meshData.indices.resize(c_index + 3*((2*rings-1)*circlePoints + (makeClosed ? circlePoints-2 : 0)));

    float c_angle = (float) M_PI * 2.0f / circlePoints;

    for (int i = 0; i < rings; ++i) {
        float r_ang = i * (float) M_PI / (2*rings);
        float y_pos = glm::sin(r_ang);
        float r_rad = glm::cos(r_ang);

        for (int j = 0; j < circlePoints; ++j) {
            glm::vec3 n = glm::vec3(r_rad * glm::sin(j*c_angle), y_pos, r_rad * glm::cos(j*c_angle));

            m_meshData.vertices[c_vertexIndex] = radius * n;
            m_meshData.normals[c_vertexIndex] = n;

            ++c_vertexIndex;

            if (i == rings-1) {
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + j);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + rings*circlePoints);
            }

            if (i > 0) {
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + j);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + (j+1)%circlePoints);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + j);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + j);
            }
        }
    }

    m_meshData.vertices[c_vertexIndex] = glm::vec3(0, radius, 0);
    m_meshData.normals[c_vertexIndex] = glm::vec3(0, 1, 0);
    ++c_vertexIndex;

    if (makeClosed) {
        uint32_t baseIndex = c_vertexIndex;
        for (int i = 0; i < circlePoints; ++i) {
            m_meshData.vertices[c_vertexIndex] = m_meshData.vertices[indexOffset + circlePoints - i - 1];
            m_meshData.normals[c_vertexIndex] = glm::vec3(0, -1, 0);

            if (i > 1) {
                m_meshData.indices[c_index++] = c_vertexIndex;
                m_meshData.indices[c_index++] = baseIndex;
                m_meshData.indices[c_index++] = c_vertexIndex-1;
            }

            ++c_vertexIndex;
        }
    }

    return *this;
}

MeshBuilder& MeshBuilder::sphere(float radius, int circlePoints, int rings) {
    if (circlePoints < 3 || rings < 1) {
        return *this;
    }

    size_t nVertices = static_cast<size_t> (rings * circlePoints + 2);
    size_t c_vertexIndex = m_meshData.vertices.size();
    size_t c_index = m_meshData.indices.size();
    size_t indexOffset = c_vertexIndex;

    m_meshData.vertices.resize(c_vertexIndex + nVertices);
    m_meshData.normals.resize(m_meshData.vertices.size());
    m_meshData.indices.resize(c_index + 6*rings*circlePoints);

    float c_angle = (float) M_PI * 2.0f / circlePoints;

    m_meshData.vertices[c_vertexIndex] = glm::vec3(0, -radius, 0);
    m_meshData.normals[c_vertexIndex] = glm::vec3(0, -1, 0);
    ++c_vertexIndex;

    for (int i = 0; i < rings; ++i) {
        float r_ang = (i+1) * (float) M_PI / (rings+1) - (float) M_PI / 2.0f;
        float y_pos = glm::sin(r_ang);
        float r_rad = glm::cos(r_ang);

        for (int j = 0; j < circlePoints; ++j) {
            glm::vec3 n = glm::vec3(r_rad * glm::sin(j*c_angle), y_pos, r_rad * glm::cos(j*c_angle));

            m_meshData.vertices[c_vertexIndex] = radius * n;
            m_meshData.normals[c_vertexIndex] = n;

            ++c_vertexIndex;

            if (i == 0) {
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + j + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints + 1);
            }

            if (i == rings-1) {
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + j + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + rings*circlePoints + 1);
            }

            if (i > 0) {
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + j + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + (j+1)%circlePoints + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + j + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + j + 1);
            }
        }
    }

    m_meshData.vertices[c_vertexIndex] = glm::vec3(0, radius, 0);
    m_meshData.normals[c_vertexIndex] = glm::vec3(0, 1, 0);
    ++c_vertexIndex;

    return *this;
}

MeshBuilder& MeshBuilder::capsule(float radius, float height, int circlePoints, int segments, int hemisphereRings) {
    if (segments == 0) {
        return sphere(radius, circlePoints, 2*hemisphereRings-1);
    }
    if (circlePoints < 3) {
        return *this;
    }

    size_t nVertices = static_cast<size_t> ((2*hemisphereRings+segments-1) * circlePoints + 2);
    size_t c_vertexIndex = m_meshData.vertices.size();
    size_t c_index = m_meshData.indices.size();
    size_t indexOffset = c_vertexIndex;

    m_meshData.vertices.resize(c_vertexIndex + nVertices);
    m_meshData.normals.resize(m_meshData.vertices.size());
    m_meshData.indices.resize(c_index + 6*(2*hemisphereRings+segments)*circlePoints);

    float c_angle = (float) M_PI * 2.0f / circlePoints;

    m_meshData.vertices[c_vertexIndex] = glm::vec3(0, -height/2.0f, 0);
    m_meshData.normals[c_vertexIndex] = glm::vec3(0, -1, 0);
    ++c_vertexIndex;

    for (int i = 0; i < 2*hemisphereRings + segments-1; ++i) {
        float y_pos = 0, r_rad = 0;

        if (i < hemisphereRings-1) {  // Bottom hemisphere
            float r_ang = (i+1) * (float) M_PI / (2 * hemisphereRings) - (float) M_PI / 2.0f;
            y_pos = glm::sin(r_ang);
            r_rad = glm::cos(r_ang);
        } else if (i > hemisphereRings + segments - 1) {  // Top hemisphere
            float r_ang = (i-hemisphereRings-segments+1) * (float) M_PI / (2 * hemisphereRings);
            y_pos = glm::sin(r_ang);
            r_rad = glm::cos(r_ang);
        }

        for (int j = 0; j < circlePoints; ++j) {
            if (i < hemisphereRings-1) {  // Bottom hemisphere
                glm::vec3 n = glm::vec3(r_rad * glm::sin(j * c_angle), y_pos, r_rad * glm::cos(j * c_angle));
                m_meshData.vertices[c_vertexIndex] = radius * n - glm::vec3(0, height/2.0f - radius, 0);
                m_meshData.normals[c_vertexIndex] = n;
            } else if (i > hemisphereRings + segments - 1) {  // Top hemisphere
                glm::vec3 n = glm::vec3(r_rad * glm::sin(j * c_angle), y_pos, r_rad * glm::cos(j * c_angle));
                m_meshData.vertices[c_vertexIndex] = radius * n + glm::vec3(0, height/2.0f - radius, 0);
                m_meshData.normals[c_vertexIndex] = n;
            } else if (i == hemisphereRings-1) {  // First ring of middle cylinder
                glm::vec3 n = glm::vec3(glm::sin(j * c_angle), 0, glm::cos(j*c_angle));
                m_meshData.vertices[c_vertexIndex] = radius * n - glm::vec3(0, height/2.0f - radius, 0);
                m_meshData.normals[c_vertexIndex] = n;
            } else {  // Other rings of middle cylinder
                m_meshData.vertices[c_vertexIndex] = m_meshData.vertices[c_vertexIndex-circlePoints] + glm::vec3(0, (height - 2*radius) / segments, 0);
                m_meshData.normals[c_vertexIndex] = m_meshData.normals[c_vertexIndex-circlePoints];
            }
            ++c_vertexIndex;

            if (i == 0) {
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + j + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints + 1);
            }

            if (i == 2*hemisphereRings + segments - 2) {
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + j + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (2*hemisphereRings+segments-1)*circlePoints + 1);
            }

            if (i > 0) {
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + j + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + (j+1)%circlePoints + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + (i-1)*circlePoints + j + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + (j+1)%circlePoints + 1);
                m_meshData.indices[c_index++] = static_cast<uint32_t>(indexOffset + i*circlePoints + j + 1);
            }
        }
    }

    m_meshData.vertices[c_vertexIndex] = glm::vec3(0, height/2.0f, 0);
    m_meshData.normals[c_vertexIndex] = glm::vec3(0, 1, 0);
    ++c_vertexIndex;

    return *this;
}

MeshBuilder& MeshBuilder::plane(float width) {
    uint32_t indexOffset = m_meshData.vertices.size();

    glm::vec3 vertices[] {
        {-width/2, 0, -width/2}, {-width/2, 0, width/2}, {width/2, 0, -width/2}, {width/2, 0, width/2}};
    uint32_t indices[] {
        indexOffset, indexOffset+1, indexOffset+3, indexOffset, indexOffset+3, indexOffset+2};

    m_meshData.vertices.insert(m_meshData.vertices.end(), vertices, vertices+4);
    m_meshData.normals.resize(m_meshData.vertices.size());
    std::fill(m_meshData.normals.begin()+indexOffset, m_meshData.normals.end(), glm::vec3(0, 1, 0));
    m_meshData.indices.insert(m_meshData.indices.end(), indices, indices+6);

    return *this;
}

MeshBuilder& MeshBuilder::cube(float width) {
    size_t indexOffset = m_meshData.vertices.size();

    glm::vec3 vertices[] {
        {-width/2, -width/2, -width/2}, { width/2, -width/2, -width/2},
        { width/2, -width/2,  width/2}, {-width/2, -width/2,  width/2},
        {-width/2, -width/2, -width/2}, {-width/2, -width/2,  width/2},
        {-width/2,  width/2,  width/2}, {-width/2,  width/2, -width/2},
        {-width/2, -width/2, -width/2}, {-width/2,  width/2, -width/2},
        { width/2,  width/2, -width/2}, { width/2, -width/2, -width/2},
        {-width/2,  width/2, -width/2}, {-width/2,  width/2,  width/2},
        { width/2,  width/2,  width/2}, { width/2,  width/2, -width/2},
        { width/2, -width/2, -width/2}, { width/2,  width/2, -width/2},
        { width/2,  width/2,  width/2}, { width/2, -width/2,  width/2},
        {-width/2, -width/2,  width/2}, { width/2, -width/2,  width/2},
        { width/2,  width/2,  width/2}, {-width/2,  width/2,  width/2}};

    glm::vec3 normals[] {
        { 0, -1,  0}, { 0, -1,  0}, { 0, -1,  0}, { 0, -1,  0},
        {-1,  0,  0}, {-1,  0,  0}, {-1,  0,  0}, {-1,  0,  0},
        { 0,  0, -1}, { 0,  0, -1}, { 0,  0, -1}, { 0,  0, -1},
        { 0,  1,  0}, { 0,  1,  0}, { 0,  1,  0}, { 0,  1,  0},
        { 1,  0,  0}, { 1,  0,  0}, { 1,  0,  0}, { 1,  0,  0},
        { 0,  0,  1}, { 0,  0,  1}, { 0,  0,  1}, { 0,  0,  1}};

    uint32_t indices [] {
         0,  1,  2,  2,  3,  0,
         4,  5,  6,  6,  7,  4,
         8,  9, 10, 10, 11,  8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20};

    m_meshData.vertices.insert(m_meshData.vertices.end(), vertices, vertices+24);
    m_meshData.normals.resize(m_meshData.vertices.size());
    m_meshData.normals.insert(m_meshData.normals.begin()+indexOffset, normals, normals+24);
    m_meshData.indices.insert(m_meshData.indices.end(), indices, indices+36);

    return *this;
}

MeshBuilder& MeshBuilder::uvMapPlanar(const glm::vec3& normal) {
    m_meshData.uvs.resize(m_meshData.vertices.size());

    glm::vec3 n = glm::normalize(normal);
    glm::vec3 tangent = (glm::abs(n.x) < 1.0f) ? glm::vec3(1, 0, 0) : glm::vec3(0, 0, -1);
    tangent = glm::normalize(tangent - n * glm::dot(n, tangent));
    glm::vec3 bitangent = glm::cross(n, tangent);

    glm::vec2 uvMin = std::accumulate(m_meshData.vertices.begin(), m_meshData.vertices.end(),
                                      glm::vec2(std::numeric_limits<float>::max()),
                                      [&] (const glm::vec2& acc, const glm::vec3& v) {
                                            return glm::min(acc, {glm::dot(tangent, v), glm::dot(bitangent, v)});
                                      });
    glm::vec2 uvMax = std::accumulate(m_meshData.vertices.begin(), m_meshData.vertices.end(),
                                      glm::vec2(std::numeric_limits<float>::min()),
                                      [&] (const glm::vec2& acc, const glm::vec3& v) {
                                            return glm::max(acc, {glm::dot(tangent, v), glm::dot(bitangent, v)});
                                      });
    std::transform(m_meshData.vertices.begin(), m_meshData.vertices.end(),
                   m_meshData.uvs.begin(),
                   [&] (const glm::vec3& v) {
                        return (glm::vec2(glm::dot(tangent, v), glm::dot(bitangent, v)) - uvMin) / (uvMax - uvMin);
                   });

    return *this;
}
