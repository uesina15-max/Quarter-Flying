#pragma once

#include "AssetHandle.h"
#include "../core/EngineError.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <shared_mutex>

namespace Engine
{
    // ========================================
    // Asset Registry
    // ========================================
    
    // AssetRegistry는 모든 에셋의 메타데이터를 관리하는 중앙 레지스트리입니다.
    // 경로, UUID, 타입별로 에셋을 조회할 수 있습니다.
    
    class AssetRegistry
    {
    public:
        AssetRegistry();
        ~AssetRegistry();

        // 에셋 등록
        Result<void> RegisterAsset(const AssetMetadata& metadata);
        
        // 에셋 등록 해제
        void UnregisterAsset(AssetHandle handle);
        
        // 경로로 핸들 조회
        Result<AssetHandle> FindAssetByPath(const std::string& path) const;
        
        // UUID로 핸들 조회
        Result<AssetHandle> FindAssetByUUID(const UUID& uuid) const;
        
        // 타입별 에셋 열거
        std::vector<AssetHandle> FindAssetsByType(AssetType type) const;
        
        // 메타데이터 접근 (const)
        Result<const AssetMetadata*> GetMetadata(AssetHandle handle) const;
        
        // 메타데이터 접근 (mutable)
        Result<AssetMetadata*> GetMetadata(AssetHandle handle);
        
        // 메타데이터 업데이트
        Result<void> UpdateMetadata(AssetHandle handle, const AssetMetadata& metadata);
        
        // 모든 에셋 핸들 조회
        std::vector<AssetHandle> GetAllAssets() const;
        
        // 레지스트리 크기
        size_t GetAssetCount() const;
        
        // 레지스트리 클리어
        void Clear();

    private:
        // 핸들 생성
        AssetHandle CreateHandle();
        
        // 핸들 유효성 검사
        bool IsValidHandle(AssetHandle handle) const;

    private:
        // 메타데이터 저장소 (핸들 → 메타데이터)
        std::unordered_map<AssetHandle, AssetMetadata> metadataMap;
        
        // 경로 인덱스 (경로 → 핸들)
        std::unordered_map<std::string, AssetHandle> pathIndex;
        
        // UUID 인덱스 (UUID → 핸들)
        std::unordered_map<UUID, AssetHandle> uuidIndex;
        
        // 타입 인덱스 (타입 → 핸들 목록)
        std::unordered_map<AssetType, std::vector<AssetHandle>> typeIndex;
        
        // 핸들 생성기
        uint32_t nextId;
        
        // 스레드 안전성
        mutable std::shared_mutex mutex;
    };

} // namespace Engine
