#pragma once
#include <vector>
#include "Vertex.h"
#include <string>
#include <memory>

namespace Engine
{
    class Mesh
    {
    public:
        Mesh() = default;
        ~Mesh();

        void create(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
        // GL 없이 경계만 정한다(create가 부른다, 테스트도 쓴다).
        void setLocalBounds(const std::vector<Vertex>& vertices);
        void draw() const;
        void drawInstanced(uint32_t instanceCount) const;

        static std::shared_ptr<Mesh> loadFromFile(const std::string& path);
        
        // Getters for instanced rendering
       uint32_t getIndexCount() const { return m_indexCount; }
        uint32_t getVAO() const { return m_vao; }
        uint32_t getVBO() const { return m_vbo; }
        uint32_t getEBO() const { return m_ebo; }

        // 로컬 공간 정점 경계(축 정렬 상자). 정점은 GPU로 올린 뒤 CPU에 남기지 않으므로 create 때 계산해 둔다.
        // 에디터 뷰포트 피킹(ecs/Picking.h)이 쓴다. 정점이 없으면 hasBounds()가 false다.
        bool hasBounds() const { return m_hasBounds; }
        const float* boundsMin() const { return m_boundsMin; }
        const float* boundsMax() const { return m_boundsMax; }

    private:
        bool m_hasBounds = false;
        float m_boundsMin[3] = {0, 0, 0};
        float m_boundsMax[3] = {0, 0, 0};
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        unsigned int m_ebo = 0;
        unsigned int m_indexCount = 0;
    };
}
