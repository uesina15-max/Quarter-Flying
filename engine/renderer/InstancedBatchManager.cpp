/*
 * Copyright 2026 Quarter Flying Game Engine Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "InstancedBatchManager.h"
#include "Mesh.h"
#include "Shader.h"
#include "Frustum.h"
#include "../core/logging/Logger.h"
#include <GL/glew.h>
#include <algorithm>
#include <vector>

namespace Engine
{
    // SetupInstanceVAO()가 실제로 활성화하는 vertex attribute 개수(location 0~12).
    // 내역: mesh 3개(position/normal/texcoord) + instance 10개(mat4 model 4개 + color +
    // roughness + metallic + entityId + isSelected + alpha).
    //
    // 예전에는 Initialize()/ValidateOpenGLCapabilities()가 이 값을 7("mesh 3 + mat4 4")로
    // 알고 검사하고 있었다 -- 실제 사용량보다 5개나 낮은 값이라, attribute가 7~11개인
    // 환경에서는 검사를 통과한 뒤 glEnableVertexAttribArray(8..12)가 조용히 실패해서
    // "GL 에러 없이 화면만 이상하게 그려지는" 상태가 됐을 것이다. 실제 사용량과 같은
    // 상수 하나를 세 곳이 공유하게 해서 다시 어긋나지 않도록 한다.
    // (OpenGL이 보장하는 최소값은 16이므로 13은 안전한 요구치다.)
    static constexpr uint32_t kRequiredVertexAttribs = 13;

    // ============================================================================
    // InstanceBatch Implementation
    // ============================================================================
    
    InstanceBatch::~InstanceBatch()
    {
        // Resources are managed by InstancedBatchManager
    }
    
    InstanceBatch::InstanceBatch(InstanceBatch&& other) noexcept
        : key(std::move(other.key))
        , type(other.type)
        , instanceVBO(other.instanceVBO)
        , instanceVAO(other.instanceVAO)
        , instanceData(std::move(other.instanceData))
        , entityIds(std::move(other.entityIds))
        , isDirty(other.isDirty)
        , isVisible(other.isVisible)
        , markedForDeletion(other.markedForDeletion)
        , visibleCount(other.visibleCount)
        , mesh(std::move(other.mesh))
    {
        // Reset moved-from object
        other.instanceVBO = 0;
        other.instanceVAO = 0;
        other.isDirty = false;
        other.isVisible = true;
        other.markedForDeletion = false;
        other.visibleCount = 0;
    }
    
    InstanceBatch& InstanceBatch::operator=(InstanceBatch&& other) noexcept
    {
        if (this != &other)
        {
            key = std::move(other.key);
            type = other.type;
            instanceVBO = other.instanceVBO;
            instanceVAO = other.instanceVAO;
            instanceData = std::move(other.instanceData);
            entityIds = std::move(other.entityIds);
            isDirty = other.isDirty;
            isVisible = other.isVisible;
            markedForDeletion = other.markedForDeletion;
            visibleCount = other.visibleCount;
            mesh = std::move(other.mesh);
            
            // Reset moved-from object
            other.instanceVBO = 0;
            other.instanceVAO = 0;
            other.isDirty = false;
            other.isVisible = true;
            other.markedForDeletion = false;
            other.visibleCount = 0;
        }
        return *this;
    }

    // ============================================================================
    // InstancedBatchManager Implementation
    // ============================================================================
    
    InstancedBatchManager::InstancedBatchManager()
        : initialized(false)
        , maxVertexAttribs(0)
    {
    }
    
    InstancedBatchManager::~InstancedBatchManager()
    {
        Shutdown();
    }
    
    Result<void> InstancedBatchManager::Initialize()
    {
        if (initialized)
        {
            Logger::Log(LogLevel::Warning, "InstancedBatchManager::Initialize - Already initialized");
            return {};
        }
        
        // Validate OpenGL capabilities
        if (!ValidateOpenGLCapabilities())
        {
            return MakeUnexpected(EngineErrorCode::NotSupported, 
                                  "OpenGL instance rendering capabilities insufficient", 
                                  "InstancedBatchManager");
        }
        
        maxVertexAttribs = GetMaxVertexAttributes();
        Logger::Log(LogLevel::Info, "InstancedBatchManager::Initialize - Max vertex attributes: {}", maxVertexAttribs);
        
        // SetupInstanceVAO()가 실제로 쓰는 개수(kRequiredVertexAttribs)로 검사한다.
        // 모자라면 조용히 잘못 그리지 말고, 어떤 가정이 왜 깨졌는지 메시지에 남긴다.
        if (maxVertexAttribs < kRequiredVertexAttribs)
        {
            return MakeUnexpected(EngineErrorCode::NotSupported,
                                  "Insufficient vertex attributes for instanced rendering (need " +
                                      std::to_string(kRequiredVertexAttribs) + ", got " +
                                      std::to_string(maxVertexAttribs) + ")",
                                  "InstancedBatchManager");
        }
        
        initialized = true;
        Logger::Log(LogLevel::Info, "InstancedBatchManager::Initialize - Initialized successfully");
        return {};
    }
    
    void InstancedBatchManager::Shutdown()
    {
        if (!initialized)
        {
            return;
        }
        
        Logger::Log(LogLevel::Info, "InstancedBatchManager::Shutdown - Shutting down");
        
        // Clean up all batches
        for (auto& pair : batches)
        {
            ReleaseBatchResources(*pair.second);
        }
        batches.clear();
        
        initialized = false;
        Logger::Log(LogLevel::Info, "InstancedBatchManager::Shutdown - Shutdown complete");
    }
    
    Result<void> InstancedBatchManager::CreateBatch(const InstancedBatchKey& key, BatchType type, std::shared_ptr<Mesh> mesh)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "Manager not initialized", "InstancedBatchManager");
        }
        
        if (!mesh)
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter, "Mesh is null", "InstancedBatchManager");
        }
        
        // Check if batch already exists
        if (batches.find(key) != batches.end())
        {
            Logger::Log(LogLevel::Warning, "InstancedBatchManager::CreateBatch - Batch already exists for key");
            return {};
        }
        
        // Create new batch
        auto batch = std::make_unique<InstanceBatch>();
        batch->key = key;
        batch->type = type;
        batch->mesh = mesh;
        batch->isDirty = true;
        batch->isVisible = true;
        batch->markedForDeletion = false;
        batch->visibleCount = 0;
        
        // Mark that batches need sorting
        batchesNeedSorting = true;
        
        // Generate GPU resources
        glGenBuffers(1, &batch->instanceVBO);
        glGenVertexArrays(1, &batch->instanceVAO);
        
        if (batch->instanceVBO == 0 || batch->instanceVAO == 0)
        {
            ReleaseBatchResources(*batch);
            return MakeUnexpected(EngineErrorCode::GPUResourceCreationFailed, 
                                  "Failed to generate GPU resources", 
                                  "InstancedBatchManager");
        }
        
        // Setup initial VAO
        auto result = SetupInstanceVAO(*batch);
        if (!result)
        {
            ReleaseBatchResources(*batch);
            return result;
        }
        
        batches[key] = std::move(batch);
        
        Logger::Log(LogLevel::Info, "InstancedBatchManager::CreateBatch - Created batch for mesh GUID: {}, type: {}", 
                    key.meshGuid, static_cast<uint32_t>(type));
        
        return {};
    }
    
    Result<void> InstancedBatchManager::AddInstance(const InstancedBatchKey& key, const InstanceData& data, uint32_t entityId)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "Manager not initialized", "InstancedBatchManager");
        }
        
        InstanceBatch* batch = FindBatch(key);
        if (!batch)
        {
            return MakeUnexpected(EngineErrorCode::ResourceNotFound, "Batch not found", "InstancedBatchManager");
        }
        
        batch->instanceData.push_back(data);
        batch->entityIds.push_back(entityId);
        batch->isDirty = true;
        batch->visibleCount = batch->instanceData.size();  // Initially all visible
        batchesNeedSorting = true;

        // 예전엔 여기서 인스턴스 하나마다 TRACE 로그를 남겼다("Added instance to batch,
        // total instances: N"). RenderSystem처럼 엔티티가 몇 개뿐일 땐 티가 안 났지만,
        // 파티클이 붙으면서 **프레임당 파티클 수만큼** 로그가 쏟아졌다 - 파티클 2048개
        // 기준으로 프레임당 2048줄이고, 실측에서 16초 동안 12,194줄이 쌓이는 동안
        // 6프레임밖에 못 돌았다(FPS 1). 로그 한 줄마다 문자열 포맷 + 파일 I/O가 붙는
        // 매 프레임 핫 루프였던 것이다.
        //
        // 인스턴스 단위 추적은 이 로그 없이도 stats.totalInstances로 볼 수 있으므로
        // (RendererStats), 로그를 되살리지 말 것.
        return {};
    }
    
    Result<void> InstancedBatchManager::UpdateInstance(const InstancedBatchKey& key, uint32_t instanceIndex, const InstanceData& data)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "Manager not initialized", "InstancedBatchManager");
        }
        
        InstanceBatch* batch = FindBatch(key);
        if (!batch)
        {
            return MakeUnexpected(EngineErrorCode::ResourceNotFound, "Batch not found", "InstancedBatchManager");
        }
        
        if (instanceIndex >= batch->instanceData.size())
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter, "Instance index out of range", "InstancedBatchManager");
        }
        
        batch->instanceData[instanceIndex] = data;
        batch->isDirty = true;
        
        return {};
    }
    
    void InstancedBatchManager::ClearInstances(const InstancedBatchKey& key)
    {
        InstanceBatch* batch = FindBatch(key);
        if (!batch)
        {
            return;
        }

        batch->instanceData.clear();
        batch->entityIds.clear();
        batch->isDirty = true;
        batch->visibleCount = 0;
    }

    Result<void> InstancedBatchManager::RemoveBatch(const InstancedBatchKey& key)
    {
        if (!initialized)
        {
            return MakeUnexpected(EngineErrorCode::NotInitialized, "Manager not initialized", "InstancedBatchManager");
        }
        
        auto it = batches.find(key);
        if (it == batches.end())
        {
            return MakeUnexpected(EngineErrorCode::ResourceNotFound, "Batch not found", "InstancedBatchManager");
        }
        
        ReleaseBatchResources(*it->second);
        batches.erase(it);
        batchesNeedSorting = true;
        
        Logger::Log(LogLevel::Info, "InstancedBatchManager::RemoveBatch - Removed batch for mesh GUID: {}", key.meshGuid);
        
        return {};
    }
    
    void InstancedBatchManager::SetBatchVisibility(const InstancedBatchKey& key, bool visible)
    {
        InstanceBatch* batch = FindBatch(key);
        if (batch)
        {
            batch->isVisible = visible;
            if (!visible)
            {
                batch->visibleCount = 0;
            }
            else
            {
                batch->visibleCount = batch->instanceData.size();
            }
        }
    }
    
    void InstancedBatchManager::UpdateVisibility(const Frustum& frustum)
    {
        // Implement frustum culling for each instance
        for (auto& pair : batches)
        {
            InstanceBatch& batch = *pair.second;
            if (!batch.isVisible || batch.instanceData.empty())
            {
                batch.visibleCount = 0;
                continue;
            }

            uint32_t visibleCount = 0;
            
            // Frustum culling per instance
            for (size_t i = 0; i < batch.instanceData.size(); ++i)
            {
                const InstanceData& instance = batch.instanceData[i];
                
                // Extract position from model matrix (translation component)
                glm::vec3 position(instance.model[3][0], instance.model[3][1], instance.model[3][2]);
                
                // Calculate bounding sphere radius based on mesh bounds if available
                // For now, use a conservative estimate based on scale
                float scale = glm::max(glm::max(instance.model[0][0], instance.model[1][1]), instance.model[2][2]);
                float radius = scale * 1.0f; // Assume unit sphere, adjust based on actual mesh bounds
                
                // Test against frustum
                if (frustum.IsSphereVisible(position, radius))
                {
                    visibleCount++;
                }
                else
                {
                    stats.frustumCulled++;
                }
            }
            
            batch.visibleCount = visibleCount;
            stats.culledInstances += (batch.instanceData.size() - visibleCount);
        }
    }
    
    void InstancedBatchManager::RenderBatches(PassType passType, Shader* shader)
    {
        if (!initialized || !shader)
        {
            return;
        }
        
        // Sort batches for optimal render state changes
        SortBatchesForRendering();
        
        for (auto& pair : batches)
        {
            InstanceBatch& batch = *pair.second;
            
            // Check if batch matches pass type and is visible
            if (batch.key.passType != passType || !batch.isVisible || batch.visibleCount == 0)
            {
                continue;
            }
            
            // Update dynamic batches if dirty
            if (batch.type == BatchType::Dynamic && batch.isDirty)
            {
                UploadInstanceBuffer(batch);
                batch.isDirty = false;
            }
            
            // Render the batch
            RenderBatch(batch.key, shader);
        }
    }
    
    void InstancedBatchManager::RenderBatch(const InstancedBatchKey& key, Shader* shader)
    {
        InstanceBatch* batch = FindBatch(key);
        if (!batch || !batch->mesh || !shader)
        {
            return;
        }

        // 방어적 체크: instanceVAO는 CreateBatch()가 성공해야만 0이 아닌 값으로 채워진다.
        // CreateBatch()의 Result가 무시되거나(GPU 자원 생성 실패) ReleaseBatchResources() 이후
        // 재사용된 배치가 맵에 남아 있으면 여기서 instanceVAO == 0인 채로 들어올 수 있는데,
        // 이걸 체크 없이 그냥 glBindVertexArray(0)(기본 VAO)으로 그렸다면 Mesh::drawInstanced()
        // 버그(위 CLAUDE.md "코드 품질 관례" #3 참고)와 같은 종류의 "GL 에러 없이 화면에는
        // 아무 것도(혹은 엉뚱하게) 그려지지 않는" 증상을 냈을 것이다 - 여기서 조기에 걸러서
        // 원인을 로그로 남긴다.
        if (batch->instanceVAO == 0)
        {
            Logger::Log(LogLevel::Error,
                "InstancedBatchManager::RenderBatch - instanceVAO is 0 for mesh GUID: {} (CreateBatch "
                "may have failed or this batch's GPU resources were already released) - skipping draw",
                key.meshGuid);
            return;
        }

        // Bind instance VAO (which includes mesh vertex attributes)
        glBindVertexArray(batch->instanceVAO);
        
        // Use shader
        shader->use();
        
        // Draw instanced using Mesh method
        batch->mesh->drawInstanced(batch->visibleCount);
        
        glBindVertexArray(0);
        
        // Update statistics
        stats.drawCalls++;
        stats.visibleInstances += batch->visibleCount;
        stats.totalInstances += batch->instanceData.size();
        
        if (batch->type == BatchType::Static)
        {
            stats.staticBatches++;
        }
        else
        {
            stats.dynamicBatches++;
        }
    }
    
    void InstancedBatchManager::UpdateDynamicBatches()
    {
        for (auto& pair : batches)
        {
            InstanceBatch& batch = *pair.second;
            
            if (batch.type == BatchType::Dynamic && batch.isDirty)
            {
                UploadInstanceBuffer(batch);
                batch.isDirty = false;
            }
        }
    }
    
    void InstancedBatchManager::CleanupStaleBatches()
    {
        // Remove batches with zero instances or marked for deletion
        auto it = batches.begin();
        while (it != batches.end())
        {
            if (it->second->instanceData.empty() || it->second->markedForDeletion)
            {
                // Clean up OpenGL resources if any
                if (it->second->instanceVAO != 0)
                {
                    glDeleteVertexArrays(1, &it->second->instanceVAO);
                }
                if (it->second->instanceVBO != 0)
                {
                    glDeleteBuffers(1, &it->second->instanceVBO);
                }
                
                it = batches.erase(it);
                batchesNeedSorting = true;
            }
            else
            {
                ++it;
            }
        }
    }
    
    bool InstancedBatchManager::ValidateOpenGLCapabilities()
    {
        GLint maxVertexAttribs = 0;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
        
        GLint maxVertexUniformComponents = 0;
        glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, &maxVertexUniformComponents);
        
        Logger::Log(LogLevel::Info, "InstancedBatchManager::ValidateOpenGLCapabilities - Max vertex attribs: {}, Max vertex uniform components: {}", 
                    maxVertexAttribs, maxVertexUniformComponents);
        
        // Check for instancing support
        bool hasInstancing = glewIsSupported("GL_ARB_instanced_arrays") != 0;
        
        if (!hasInstancing)
        {
            Logger::Log(LogLevel::Error, "InstancedBatchManager::ValidateOpenGLCapabilities - GL_ARB_instanced_arrays not supported");
            return false;
        }
        
        if (static_cast<uint32_t>(maxVertexAttribs) < kRequiredVertexAttribs)
        {
            Logger::Log(LogLevel::Error,
                "InstancedBatchManager::ValidateOpenGLCapabilities - Need {} vertex attributes for the "
                "instance VAO layout but this context only provides {}",
                kRequiredVertexAttribs, maxVertexAttribs);
            return false;
        }

        return true;
    }
    
    uint32_t InstancedBatchManager::GetMaxVertexAttributes()
    {
        GLint maxAttribs = 0;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
        return static_cast<uint32_t>(maxAttribs);
    }
    
    InstanceBatch* InstancedBatchManager::FindBatch(const InstancedBatchKey& key)
    {
        auto it = batches.find(key);
        return (it != batches.end()) ? it->second.get() : nullptr;
    }
    
    const InstanceBatch* InstancedBatchManager::FindBatch(const InstancedBatchKey& key) const
    {
        auto it = batches.find(key);
        return (it != batches.end()) ? it->second.get() : nullptr;
    }
    
    Result<void> InstancedBatchManager::UploadInstanceBuffer(InstanceBatch& batch)
    {
        if (batch.instanceData.empty())
        {
            return {};
        }
        
        glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);
        
        GLenum usage = (batch.type == BatchType::Static) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
        glBufferData(GL_ARRAY_BUFFER, batch.instanceData.size() * sizeof(InstanceData), 
                     batch.instanceData.data(), usage);
        
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        Logger::Log(LogLevel::Trace, "InstancedBatchManager::UploadInstanceBuffer - Uploaded {} instances", 
                    batch.instanceData.size());
        
        return {};
    }
    
    Result<void> InstancedBatchManager::SetupInstanceVAO(InstanceBatch& batch)
    {
        // Create a VAO that combines mesh vertex attributes with instance attributes
        // This approach separates instance buffer ownership from Mesh while maintaining rendering efficiency
        
        glBindVertexArray(batch.instanceVAO);
        
        // Setup mesh vertex attributes first
        if (batch.mesh)
        {
            uint32_t meshVBO = batch.mesh->getVBO();
            uint32_t meshEBO = batch.mesh->getEBO();
            
            if (meshVBO == 0 || meshEBO == 0)
            {
                Logger::Log(LogLevel::Error, "InstancedBatchManager::SetupInstanceVAO - Mesh has invalid VBO/EBO");
                return MakeUnexpected(EngineErrorCode::InvalidState, "Mesh has invalid buffers", "InstancedBatchManager");
            }
            
            // Bind mesh VBO and setup vertex attributes (same as Mesh::create)
            glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshEBO);
            
            // Position (attribute 0)
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
            glEnableVertexAttribArray(0);
            
            // Normal (attribute 1)
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            
            // TexCoords (attribute 2)
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(2);
        }
        
        // Setup instance attributes
        glBindBuffer(GL_ARRAY_BUFFER, batch.instanceVBO);
        
        // Setup instance attributes for model matrix (mat4 = 4 vec4 attributes)
        // Attribute locations 3, 4, 5, 6 for model matrix columns
        GLsizei stride = sizeof(InstanceData);
        
        // Column 0
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(InstanceData, model));
        glVertexAttribDivisor(3, 1);
        
        // Column 1
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(offsetof(InstanceData, model) + 16));
        glVertexAttribDivisor(4, 1);
        
        // Column 2
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)(offsetof(InstanceData, model) + 32));
        glVertexAttribDivisor(5, 1);
        
        // Column 3
        glEnableVertexAttribArray(6);
        glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, stride, (void*)(offsetof(InstanceData, model) + 48));
        glVertexAttribDivisor(6, 1);
        
        // Color (attribute 7)
        glEnableVertexAttribArray(7);
        glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(InstanceData, color));
        glVertexAttribDivisor(7, 1);
        
        // Roughness (attribute 8)
        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(InstanceData, roughness));
        glVertexAttribDivisor(8, 1);
        
        // Metallic (attribute 9)
        glEnableVertexAttribArray(9);
        glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(InstanceData, metallic));
        glVertexAttribDivisor(9, 1);
        
        // Entity ID (attribute 10)
        glEnableVertexAttribArray(10);
        glVertexAttribIPointer(10, 1, GL_UNSIGNED_INT, stride, (void*)offsetof(InstanceData, entityId));
        glVertexAttribDivisor(10, 1);
        
        // Is Selected (attribute 11)
        glEnableVertexAttribArray(11);
        glVertexAttribIPointer(11, 1, GL_UNSIGNED_INT, stride, (void*)offsetof(InstanceData, isSelected));
        glVertexAttribDivisor(11, 1);

        // Alpha (attribute 12) -- docs/VFX_LITE_PLAN.md §7.2.
        // 기존 셰이더(pbr_instanced.vert, SceneMeshRenderer.cpp의 인라인 셰이더)는 location
        // 0~11만 선언하고 12는 선언하지 않는데, 활성화됐지만 셰이더가 선언하지 않은
        // attribute는 GL이 그냥 무시하므로 그 셰이더들을 고칠 필요가 없다. 알파를 실제로
        // 쓰는 파티클 셰이더만 `layout(location = 12) in float aAlpha;`를 선언하면 된다.
        glEnableVertexAttribArray(12);
        glVertexAttribPointer(12, 1, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(InstanceData, alpha));
        glVertexAttribDivisor(12, 1);

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        Logger::Log(LogLevel::Info, "InstancedBatchManager::SetupInstanceVAO - Setup instance VAO with {} attributes (3 mesh + 10 instance)",
                    kRequiredVertexAttribs);
        
        return {};
    }
    
    void InstancedBatchManager::ReleaseBatchResources(InstanceBatch& batch)
    {
        if (batch.instanceVBO != 0)
        {
            glDeleteBuffers(1, &batch.instanceVBO);
            batch.instanceVBO = 0;
        }
        
        if (batch.instanceVAO != 0)
        {
            glDeleteVertexArrays(1, &batch.instanceVAO);
            batch.instanceVAO = 0;
        }
        
        batch.instanceData.clear();
        batch.entityIds.clear();
    }
    
    void InstancedBatchManager::SortBatchesForRendering()
    {
        if (!batchesNeedSorting)
        {
            return;  // Already sorted
        }
        
        // Sort batches based on:
        // 1. Material (for state change minimization)
        // 2. Mesh (for batching efficiency)
        // 3. Pass type (for rendering order)
        
        // Rebuild the sorted batch cache
        sortedBatches.clear();
        sortedBatches.reserve(batches.size());
        
        for (auto& pair : batches)
        {
            // batches maps to unique_ptr<InstanceBatch>; sortedBatches wants the raw
            // InstanceBatch* it owns, not a pointer to the unique_ptr itself (&pair.second).
            sortedBatches.emplace_back(pair.first, pair.second.get());
        }
        
        // Sort by material ID first, then mesh GUID, then pass type
        std::sort(sortedBatches.begin(), sortedBatches.end(),
            [](const auto& a, const auto& b)
            {
                // First sort by material
                if (a.first.materialId != b.first.materialId)
                {
                    return a.first.materialId < b.first.materialId;
                }
                
                // Then sort by mesh
                if (a.first.meshGuid != b.first.meshGuid)
                {
                    return a.first.meshGuid < b.first.meshGuid;
                }
                
                // Finally sort by pass type (opaque before transparent)
                return static_cast<uint32_t>(a.first.passType) < static_cast<uint32_t>(b.first.passType);
            });
        
        batchesNeedSorting = false;
    }

} // namespace Engine
