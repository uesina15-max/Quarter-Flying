#pragma once

#include "AssetHandle.h"
#include "AssetRegistry.h"
#include "../core/Subsystem.h"
#include "../core/EngineError.h"
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <future>

namespace Engine
{
    // ========================================
    // Forward Declarations
    // ========================================

    class AssetImporter;
    class AssetCache;

    // ========================================
    // Asset Manager
    // ========================================

    class AssetManager : public Subsystem
    {
    public:
        AssetManager();
        ~AssetManager() override;

        // Subsystem 인터페이스 구현
        Result<void> Initialize() override;
        void Shutdown() noexcept override;
        void Tick(float deltaTime) override;

        // 에셋 로딩 (동기)
        Result<AssetHandle> LoadAsset(const std::string& path);
        
        // 에셋 로딩 (비동기)
        std::future<Result<AssetHandle>> LoadAssetAsync(
            const std::string& path,
            std::function<void(Result<AssetHandle>)> callback = nullptr
        );
        
        // 에셋 언로드 (참조 카운트 기반)
        void UnloadAsset(AssetHandle handle);
        
        // 에셋 임포트 (소스 → 런타임)
        Result<AssetHandle> ImportAsset(
            const std::string& sourcePath,
            const ImportOptions& options = ImportOptions{}
        );
        
        // 에셋 재임포트 (소스 변경 시)
        Result<AssetHandle> ReimportAsset(AssetHandle handle);
        
        // 메타데이터 접근
        Result<const AssetMetadata*> GetMetadata(AssetHandle handle) const;
        Result<void> SetMetadata(AssetHandle handle, const AssetMetadata& metadata);
        
        // 에셋 데이터 접근
        template<typename T>
        Result<T*> GetAsset(AssetHandle handle);
        
        // 임포터 등록
        void RegisterImporter(std::unique_ptr<AssetImporter> importer);
        
        // Asset Cache 설정
        void SetAssetCache(std::unique_ptr<AssetCache> cache);
        
        // 에셋 레지스트리 접근
        AssetRegistry* GetRegistry() { return &registry; }
        const AssetRegistry* GetRegistry() const { return &registry; }

    private:
        // 내부 로딩 구현
        Result<AssetHandle> LoadAssetInternal(const std::string& path);
        
        // 캐시에서 로드 시도
        Result<ImportedAssetData> LoadFromCache(const UUID& uuid);
        
        // 캐시에 저장
        void SaveToCache(const UUID& uuid, const ImportedAssetData& data);
        
        // 적절한 임포터 찾기
        AssetImporter* FindImporter(const std::string& extension);
        
        // 소스 파일 해시 계산
        uint64_t CalculateSourceHash(const std::string& path) const;
        
        // 에셋 데이터 저장소
        std::unordered_map<AssetHandle, std::shared_ptr<void>> assetDataMap;
        
        // 스레드 안전성
        mutable std::shared_mutex dataMutex;
        std::mutex importMutex;

    private:
        AssetRegistry registry;
        std::vector<std::unique_ptr<AssetImporter>> importers;
        std::unique_ptr<AssetCache> assetCache;
        
        bool initialized;
    };

    // 예전에는 선언만 있고 정의가 없어서, 쓰는 순간 링크 에러가 났다(이 모듈이 어디에도 연결되지 않아 드러나지 않음).
    // T는 해당 에셋 타입의 runtimeData 타입이어야 한다(텍스처는 TextureData). 타입을 검사할 수 없으므로 호출자가 맞춘다.
    template<typename T>
    Result<T*> AssetManager::GetAsset(AssetHandle handle)
    {
        std::shared_lock<std::shared_mutex> lock(dataMutex);
        auto it = assetDataMap.find(handle);
        if (it == assetDataMap.end() || !it->second)
        {
            return MakeError(EngineErrorCode::AssetNotFound,
                             "No runtime data for asset handle (not loaded, or the importer produced none)",
                             "AssetManager");
        }
        return static_cast<T*>(it->second.get());
    }

} // namespace Engine
