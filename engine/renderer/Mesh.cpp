#include "Mesh.h"
#include <GL/glew.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include "../core/logging/Logger.h"
#include <unordered_map>
#include "ObjLoaderHelper.h"

namespace Engine
{
    Mesh::~Mesh()
    {
        if (m_ebo) glDeleteBuffers(1, &m_ebo);
        if (m_vbo) glDeleteBuffers(1, &m_vbo);
        if (m_vao) glDeleteVertexArrays(1, &m_vao);
    }

    void Mesh::create(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    {
        m_indexCount = (unsigned int)indices.size();

        glGenVertexArrays(1, &m_vao);
        glGenBuffers(1, &m_vbo);
        glGenBuffers(1, &m_ebo);

        glBindVertexArray(m_vao);

        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glBindVertexArray(0);
    }

    void Mesh::draw() const
    {
        glBindVertexArray(m_vao);
        glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void Mesh::drawInstanced(uint32_t instanceCount) const
    {
        glBindVertexArray(m_vao);
        glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0, instanceCount);
        glBindVertexArray(0);
    }

    static std::unordered_map<std::string, std::shared_ptr<Mesh>> s_meshCache;

    std::shared_ptr<Mesh> Mesh::loadFromFile(const std::string& path)
    {
        if (s_meshCache.find(path) != s_meshCache.end())
        {
            Logger::Log(LogLevel::Info, "Mesh::loadFromFile - Cache HIT for: {} (returning shared_ptr)", path);
            return s_meshCache[path];
        }

        Logger::Log(LogLevel::Info, "Mesh::loadFromFile - Cache MISS for: {} (loading new mesh)", path);

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()))
        {
            Logger::Error("Failed to load obj: {} - {}", path, err);
            return nullptr;
        }

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        ObjLoaderHelper::ProcessAndInterleave(attrib, shapes, vertices, indices);

        auto mesh = std::make_shared<Mesh>();
        mesh->create(vertices, indices);
        s_meshCache[path] = mesh;
        
        Logger::Log(LogLevel::Info, "Mesh::loadFromFile - Successfully loaded and cached mesh: {} (vertices: {}, indices: {})", 
                    path, vertices.size(), indices.size());
        Logger::Log(LogLevel::Info, "Mesh::loadFromFile - Current cache size: {} meshes", s_meshCache.size());
        
        return mesh;
    }
}
