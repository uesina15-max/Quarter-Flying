#include "Mesh.h"
#include <GL/glew.h>
// tinyobjloader's vendored fast_float fails to compile under MSVC 19.44 (error C3615,
// reproduced even under C++20). Its official opt-out disables the fast_float path in
// favor of the built-in parser -- see cmake/Dependencies.cmake, which defines this
// PUBLIC on the tinyobjloader target (propagates here via quarterflying_engine's link). Guarded
// with #ifndef so this file still protects itself if that ever changes.
#ifndef TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#endif
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
        // 주의: 여기서 m_vao(이 메시 자체의 3-attribute짜리 VAO - position/normal/uv뿐,
        // 인스턴스 attribute 없음)를 bind하면 안 된다. 유일한 호출부인
        // InstancedBatchManager::RenderBatch()가 이미 인스턴스 attribute(model 행렬/
        // color/roughness/...)까지 포함하는 자신의 VAO(batch->instanceVAO, mesh의 VBO/EBO를
        // 공유하도록 SetupInstanceVAO()가 구성함)를 bind해둔 상태에서 이 함수를 부른다.
        // 예전엔 여기서 m_vao로 다시 bind해버려서 그 인스턴스 attribute들이 전부 안 잡힌
        // 상태(model 행렬이 정의되지 않은 값 - 사실상 축퇴 행렬)로 그려지고 있었다.
        // draw call 자체는 GL 에러 없이 "성공"하기 때문에 화면에 아무 것도 안 보이는데도
        // 원인 추적이 어려웠던 버그 - RenderSystem이 생기기 전까지 인스턴스 렌더링 경로가
        // 한 번도 실제로 실행된 적이 없어서 발견되지 않았다(CLAUDE.md의 "InstanceData GPU
        // 레이아웃 미검증" 항목과 같은 뿌리).
        glDrawElementsInstanced(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0, instanceCount);
    }

    // 캐시는 여기 두지 않는다. 예전에는 프로세스 전역 static 캐시(s_meshCache)가 있었는데, GL VAO는
    // 컨텍스트끼리 공유되지 않는다. 에디터에는 Engine이 둘(Scene 뷰포트 + Play 뷰포트, 각자 GL
    // 컨텍스트)이라, 두 번째 Engine이 같은 경로를 요청하면 첫 번째 컨텍스트에서 만든 VAO를 받아 엉뚱하게
    // 그렸을 것이다(GL 에러 없이 틀리게 그려지는 종류). 또 static 소멸 시점에는 컨텍스트가 이미 없다.
    // 호출자가 한 번도 없어서 드러나지 않았다. 캐시는 컨텍스트 단위로 소유하는 쪽이 맡는다
    // (RenderSystem::ResolveMeshHandle - World마다, 즉 Engine과 컨텍스트마다 하나).
    std::shared_ptr<Mesh> Mesh::loadFromFile(const std::string& path)
    {
        Logger::Log(LogLevel::Info, "Mesh::loadFromFile - Loading: {}", path);

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

        if (vertices.empty() || indices.empty())
        {
            Logger::Error("Mesh::loadFromFile - '{}' parsed but contains no triangles", path);
            return nullptr;
        }

        auto mesh = std::make_shared<Mesh>();
        mesh->create(vertices, indices);

        Logger::Log(LogLevel::Info, "Mesh::loadFromFile - Loaded mesh: {} (vertices: {}, indices: {})",
                    path, vertices.size(), indices.size());

        return mesh;
    }
}
