#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <cstddef>   // offsetof -- 아래 InstanceData 레이아웃 static_assert에서 사용
#include "RenderBatchPolicy.h"
#include "../core/EngineError.h"

namespace Engine
{
    // Forward declarations
    class Mesh;
    class Shader;

    // ============================================================================
    // Pass Type Enumeration
    // ============================================================================
    enum class PassType : uint32_t
    {
        ForwardOpaque = 0,
        ForwardTransparent = 1,
        DeferredGeometry = 2,
        DeferredLighting = 3,
        Shadow = 4,
        Custom = 5
    };

    // ============================================================================
    // Material Layout Enumeration
    // ============================================================================
    enum class MaterialLayout : uint32_t
    {
        PBR = 0,
        Phong = 1,
        Unlit = 2,
        Custom = 3
    };

    // ============================================================================
    // Enhanced Batch Key for Instance Rendering
    // ============================================================================
    struct InstancedBatchKey
    {
        uint64_t meshGuid{0};           // Unique mesh identifier
        uint64_t materialId{0};         // Unique material identifier
        uint32_t shaderId{0};           // Shader program ID
        PassType passType{PassType::ForwardOpaque};
        MaterialLayout materialLayout{MaterialLayout::PBR};
        PipelineFeature features{PipelineFeature::None};

        bool operator==(const InstancedBatchKey& other) const
        {
            return meshGuid == other.meshGuid &&
                   materialId == other.materialId &&
                   shaderId == other.shaderId &&
                   passType == other.passType &&
                   materialLayout == other.materialLayout &&
                   features == other.features;
        }

        // Hash function support
        size_t hash() const noexcept
        {
            size_t h1 = std::hash<uint64_t>{}(meshGuid);
            size_t h2 = std::hash<uint64_t>{}(materialId);
            size_t h3 = std::hash<uint32_t>{}(shaderId);
            size_t h4 = std::hash<uint32_t>{}(static_cast<uint32_t>(passType));
            size_t h5 = std::hash<uint32_t>{}(static_cast<uint32_t>(materialLayout));
            size_t h6 = std::hash<uint32_t>{}(static_cast<uint32_t>(features));
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5);
        }
    };

} // namespace Engine

// Hash specialization for std::unordered_map. This must be defined directly in
// namespace std (via reopening it here) rather than nested inside namespace Engine --
// MSVC rejects a `std::hash<T>` specialization written lexically inside another
// namespace with C2888 ("cannot define symbol inside namespace 'Engine'").
namespace std
{
    template <>
    struct hash<Engine::InstancedBatchKey>
    {
        size_t operator()(const Engine::InstancedBatchKey& key) const noexcept
        {
            return key.hash();
        }
    };
}

namespace Engine
{
    // ============================================================================
    // Instance Data Structure (GPU-aligned)
    // ============================================================================
    struct InstanceData
    {
        glm::mat4 model;          // 64 bytes - Model transformation matrix
        glm::vec3 color;          // 12 bytes - Material color (for scalar/vector materials)
        float roughness;          // 4 bytes  - Roughness value
        float metallic;           // 4 bytes  - Metallic value
        uint32_t entityId;        // 4 bytes  - Entity ID for picking/editor
        uint32_t isSelected;      // 4 bytes  - Selection flag for highlighting
        
        // 여기까지 92 bytes. 92는 16의 배수가 아니다(다음 경계는 96, 104가 아니다) --
        // 원래 `_padding[3]`(12바이트)는 104로 오버슈트해서 아래 static_assert를 실제로
        // 평가할 수 있게 되자마자 걸렸다. 96 경계에 닿는 데는 4바이트면 충분하다.
        //
        // 그 4바이트를 이제 alpha가 쓴다(docs/VFX_LITE_PLAN.md §7.2 — 파티클의 Alpha Fade를
        // 실을 자리가 인스턴스 데이터에 없었다: color가 vec3라 알파 채널이 없다).
        // 새 바이트를 뒤에 붙이지 않고 padding 자리를 그대로 재사용한 이유:
        //   - sizeof가 96으로 불변 -> 아래 16바이트 정렬 static_assert도, VBO stride
        //     (SetupInstanceVAO의 `GLsizei stride = sizeof(InstanceData)`)도 그대로다.
        //   - 앞 필드들의 offset이 전부 불변 -> SetupInstanceVAO()의 attribute 3~11이
        //     한 줄도 바뀌지 않는다. 즉 기존 인스턴스 렌더링 경로는 무영향이다.
        //
        // 기본값이 1.0f(완전 불투명)인 것이 중요하다: 엔진 전체에서 이 구조체를 만드는
        // 유일한 지점인 RenderSystem.cpp의 `InstanceData data{};`가 alpha를 대입하지 않으므로,
        // 기본 멤버 초기자가 그대로 쓰여 기존 불투명 렌더링 결과가 변하지 않는다.
        // (예전에 이 4바이트는 초기화되지 않은 값인 채로 GPU에 업로드되고 있었다 --
        // 읽는 셰이더가 없어서 무해했을 뿐이다.)
        float alpha{1.0f};        // 4 bytes -> 92 + 4 = 96 (16-byte aligned), 셰이더 location 12
    };

    // static_assert(sizeof(T)...) must come after T's own closing brace -- InstanceData
    // is still an incomplete type inside its own definition, so sizeof(InstanceData)
    // there fails with C2027 ("undefined type 'Engine::InstanceData' used").
    static_assert(sizeof(InstanceData) % 16 == 0, "InstanceData must be 16-byte aligned for GPU compatibility");

    // 아래 3개는 "C++ 구조체 레이아웃"과 "셰이더의 layout(location=...)"이 어긋나는 사고를
    // 컴파일 타임에 잡기 위한 것이다. 이 종류의 불일치는 GL 에러를 내지 않는다 -- 그냥
    // 엉뚱한 바이트를 인스턴스 attribute로 읽어서 화면만 이상해지기 때문에
    // (Mesh::drawInstanced 버그와 같은 유형) 실행 중에 원인을 찾기가 매우 어렵다.
    // 레이아웃을 바꿔야 한다면 SetupInstanceVAO()의 glVertexAttribPointer 호출과
    // 인스턴스 셰이더들(assets/shaders/pbr_instanced.vert, SceneMeshRenderer.cpp의
    // 인라인 셰이더)을 반드시 함께 고쳐야 한다.
    static_assert(sizeof(InstanceData) == 96,
                  "InstanceData size changed -- update SetupInstanceVAO() and every instanced shader");
    static_assert(offsetof(InstanceData, color) == 64,
                  "instance color moved -- shader location 7 expects offset 64");
    static_assert(offsetof(InstanceData, alpha) == 92,
                  "instance alpha moved -- shader location 12 expects offset 92");

    // ============================================================================
    // Batch Type Enumeration
    // ============================================================================
    enum class BatchType : uint32_t
    {
        Static = 0,    // Immutable data (map objects, etc.)
        Dynamic = 1    // Mutable data (moving objects, editor changes)
    };

    // ============================================================================
    // Instance Batch Container
    // ============================================================================
    struct InstanceBatch
    {
        InstancedBatchKey key;
        BatchType type;
        
        // GPU resources
        uint32_t instanceVBO{0};
        uint32_t instanceVAO{0};
        
        // CPU data
        std::vector<InstanceData> instanceData;
        std::vector<uint32_t> entityIds;  // InstanceID -> EntityID mapping for picking
        
        // State tracking
        bool isDirty{true};
        bool isVisible{true};
        bool markedForDeletion{false};
        uint32_t visibleCount{0};

        // frustum 컬링 결과 보이는 인스턴스만 모은 목록. cullingApplied가 true면 GPU에는 이것을 올린다
        // (UpdateVisibility / UploadInstanceBuffer 주석 참고).
        std::vector<InstanceData> visibleInstanceData;
        bool cullingApplied{false};

        // 이 배치를 그릴 때 바인딩할 GL 텍스처(0 = 텍스처 없음, 인스턴스 색만). SetBatchTexture로 설정한다.
        uint32_t texture{0};
        
        // Mesh reference (shared_ptr for dedup)
        std::shared_ptr<Mesh> mesh;
        
        InstanceBatch() = default;
        ~InstanceBatch();
        
        // Prevent copying
        InstanceBatch(const InstanceBatch&) = delete;
        InstanceBatch& operator=(const InstanceBatch&) = delete;
        
        // Allow moving
        InstanceBatch(InstanceBatch&& other) noexcept;
        InstanceBatch& operator=(InstanceBatch&& other) noexcept;
    };

    // ============================================================================
    // Renderer Statistics
    // ============================================================================
    struct RendererStats
    {
        uint32_t drawCalls{0};
        uint32_t totalInstances{0};
        uint32_t visibleInstances{0};
        uint32_t staticBatches{0};
        uint32_t dynamicBatches{0};
        uint32_t culledInstances{0};
        uint32_t frustumCulled{0};
        uint32_t distanceCulled{0};
        uint32_t occlusionCulled{0};
        float cullingTimeMs{0.0f};
        
        void Reset()
        {
            drawCalls = 0;
            totalInstances = 0;
            visibleInstances = 0;
            staticBatches = 0;
            dynamicBatches = 0;
            culledInstances = 0;
            frustumCulled = 0;
            distanceCulled = 0;
            occlusionCulled = 0;
            cullingTimeMs = 0.0f;
        }
    };

    // ============================================================================
    // Instanced Batch Manager
    // ============================================================================
    class InstancedBatchManager
    {
    public:
        InstancedBatchManager();
        ~InstancedBatchManager();

        // Initialization
        Result<void> Initialize();
        void Shutdown();

        // Batch management
        Result<void> CreateBatch(const InstancedBatchKey& key, BatchType type, std::shared_ptr<Mesh> mesh);
        Result<void> AddInstance(const InstancedBatchKey& key, const InstanceData& data, uint32_t entityId);
        Result<void> UpdateInstance(const InstancedBatchKey& key, uint32_t instanceIndex, const InstanceData& data);
        Result<void> RemoveBatch(const InstancedBatchKey& key);

        // 배치의 GPU 자원(VAO/VBO)은 그대로 두고 CPU 인스턴스 목록만 비운다. RenderSystem처럼
        // "매 프레임 이 배치에 속한 엔티티를 처음부터 다시 나열"하는 소비자를 위한 것 -
        // AddInstance는 중복 방지 없이 그냥 push_back이라, 매 프레임 호출 전에 이걸로
        // 비우지 않으면 인스턴스가 프레임마다 계속 누적된다. 존재하지 않는 키면 아무 일도 안 함.
        void ClearInstances(const InstancedBatchKey& key);
        
        // Visibility and culling
        void SetBatchVisibility(const InstancedBatchKey& key, bool visible);

        // 배치의 텍스처(GL id, 0 = 없음). RenderBatch가 unit 0에 바인딩하고 셰이더에 uHasTexture/uTexture로 알린다.
        void SetBatchTexture(const InstancedBatchKey& key, uint32_t glTexture);
        void UpdateVisibility(const class Frustum& frustum);
        
        // Rendering
        void RenderBatches(PassType passType, Shader* shader);
        void RenderBatch(const InstancedBatchKey& key, Shader* shader);
        
        // Lifecycle management
        void UpdateDynamicBatches();  // Upload dirty dynamic data to GPU
        void CleanupStaleBatches();
        void SortBatchesForRendering();  // Sort batches for optimal rendering order
        
        // 읽기 전용 조회(테스트/진단용). 없으면 nullptr.
        const InstanceBatch* GetBatch(const InstancedBatchKey& key) const { return FindBatch(key); }

        // Statistics
        const RendererStats& GetStats() const { return stats; }
        void ResetStats() { stats.Reset(); }
        
        // Validation
        static bool ValidateOpenGLCapabilities();
        static uint32_t GetMaxVertexAttributes();

    private:
        // Internal helpers
        InstanceBatch* FindBatch(const InstancedBatchKey& key);
        const InstanceBatch* FindBatch(const InstancedBatchKey& key) const;
        
        // Sorted batch cache for rendering
        std::vector<std::pair<InstancedBatchKey, InstanceBatch*>> sortedBatches;
        bool batchesNeedSorting{true};
        
        Result<void> UploadInstanceBuffer(InstanceBatch& batch);
        Result<void> SetupInstanceVAO(InstanceBatch& batch);
        void ReleaseBatchResources(InstanceBatch& batch);

        // NOTE: SortBatchesForRendering() was declared a second time here (duplicate of
        // the public declaration above) -- MSVC rejects that as C2535 ("member function
        // already defined or declared"). The public declaration is the one used.

    private:
        std::unordered_map<InstancedBatchKey, std::unique_ptr<InstanceBatch>> batches;
        RendererStats stats;
        
        bool initialized;
        uint32_t maxVertexAttribs;
    };

} // namespace Engine
