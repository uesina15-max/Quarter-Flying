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
        void draw() const;
        void drawInstanced(uint32_t instanceCount) const;

        static std::shared_ptr<Mesh> loadFromFile(const std::string& path);
        
        // Getters for instanced rendering
       uint32_t getIndexCount() const { return m_indexCount; }
        uint32_t getVAO() const { return m_vao; }
        uint32_t getVBO() const { return m_vbo; }
        uint32_t getEBO() const { return m_ebo; }

    private:
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        unsigned int m_ebo = 0;
        unsigned int m_indexCount = 0;
    };
}
