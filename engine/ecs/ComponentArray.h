#pragma once

#include "Entity.h"
#include <vector>
#include <unordered_map>
#include <cassert>
#include <algorithm>
#include <utility>

namespace Engine
{
    // Base class for type-erased component storage
    class ComponentArray
    {
    public:
        virtual ~ComponentArray() = default;
        virtual void Remove(EntityID entity) = 0;
        virtual bool Has(EntityID entity) const = 0;
    };

    // Typed component array using Sparse Set structure
    // Guarantees contiguous memory for cache-friendly iteration
    template<typename T>
    class TypedComponentArray : public ComponentArray
    {
    public:
        void Add(EntityID entity, const T& component)
        {
            assert(entityToIndex.find(entity) == entityToIndex.end() && "Component already exists for entity");

            size_t newIndex = components.size();
            entityToIndex[entity] = newIndex;
            entityIDs.push_back(entity);
            components.push_back(component);
        }

        void Remove(EntityID entity) override
        {
            auto it = entityToIndex.find(entity);
            if (it == entityToIndex.end())
            {
                return; // Component doesn't exist
            }

            size_t indexToRemove = it->second;
            size_t lastIndex = components.size() - 1;

            // Swap with last element for O(1) removal
            if (indexToRemove != lastIndex)
            {
                components[indexToRemove] = components[lastIndex];
                entityIDs[indexToRemove] = entityIDs[lastIndex];
                entityToIndex[entityIDs[lastIndex]] = indexToRemove;
            }

            components.pop_back();
            entityIDs.pop_back();
            entityToIndex.erase(entity);
        }

        T* Get(EntityID entity)
        {
            auto it = entityToIndex.find(entity);
            if (it == entityToIndex.end())
            {
                return nullptr;
            }
            return &components[it->second];
        }

        const T* Get(EntityID entity) const
        {
            auto it = entityToIndex.find(entity);
            if (it == entityToIndex.end())
            {
                return nullptr;
            }
            return &components[it->second];
        }

        bool Has(EntityID entity) const override
        {
            return entityToIndex.find(entity) != entityToIndex.end();
        }

        // Get contiguous array for cache-friendly iteration
        std::vector<T>& GetDenseArray() { return components; }
        const std::vector<T>& GetDenseArray() const { return components; }

        // Get entity IDs corresponding to components
        std::vector<EntityID>& GetEntityIDs() { return entityIDs; }
        const std::vector<EntityID>& GetEntityIDs() const { return entityIDs; }

        size_t Size() const { return components.size(); }

        // ========================================
        // Chunk-based Parallel Processing Support
        // ========================================
        
        // Component 배열을 청크로 분할하여 병렬 처리 지원
        // chunkStart: 청크 시작 인덱스
        // chunkSize: 청크 크기
        // 반환값: 청크 내 Component들의 포인터 배열
        std::vector<T*> GetChunk(size_t chunkStart, size_t chunkSize)
        {
            std::vector<T*> chunk;
            size_t endIndex = std::min(chunkStart + chunkSize, components.size());
            
            if (chunkStart >= components.size())
            {
                return chunk; // 빈 청크 반환
            }
            
            chunk.reserve(endIndex - chunkStart);
            for (size_t i = chunkStart; i < endIndex; ++i)
            {
                chunk.push_back(&components[i]);
            }
            
            return chunk;
        }

        // 청크에 해당하는 Entity ID들 반환
        std::vector<EntityID> GetChunkEntityIDs(size_t chunkStart, size_t chunkSize) const
        {
            std::vector<EntityID> chunkEntities;
            size_t endIndex = std::min(chunkStart + chunkSize, entityIDs.size());
            
            if (chunkStart >= entityIDs.size())
            {
                return chunkEntities; // 빈 배열 반환
            }
            
            chunkEntities.reserve(endIndex - chunkStart);
            for (size_t i = chunkStart; i < endIndex; ++i)
            {
                chunkEntities.push_back(entityIDs[i]);
            }
            
            return chunkEntities;
        }

        // 전체 배열을 지정된 청크 크기로 분할했을 때의 청크 개수 반환
        size_t GetChunkCount(size_t chunkSize) const
        {
            if (chunkSize == 0 || components.empty())
            {
                return 0;
            }
            return (components.size() + chunkSize - 1) / chunkSize; // 올림 나눗셈
        }

        // 최적 청크 크기 계산 (캐시 라인과 워커 스레드 수 고려)
        static size_t CalculateOptimalChunkSize(size_t totalElements, size_t workerThreadCount = 4)
        {
            if (totalElements == 0)
            {
                return 0;
            }
            
            // 기본 최소/최대 청크 크기
            const size_t MIN_CHUNK_SIZE = 64;   // 캐시 라인 고려
            const size_t MAX_CHUNK_SIZE = 4096; // 메모리 지역성 고려
            
            // 워커 스레드 수의 2-4배 정도의 청크를 생성하여 로드 밸런싱 개선
            size_t targetChunkCount = workerThreadCount * 3;
            size_t calculatedChunkSize = totalElements / targetChunkCount;
            
            // 최소/최대 범위 내로 제한
            calculatedChunkSize = std::max(calculatedChunkSize, MIN_CHUNK_SIZE);
            calculatedChunkSize = std::min(calculatedChunkSize, MAX_CHUNK_SIZE);
            
            return calculatedChunkSize;
        }

        // 청크 기반 반복자 지원
        struct ChunkIterator
        {
            TypedComponentArray<T>* array;
            size_t chunkSize;
            size_t currentChunk;
            size_t totalChunks;
            
            ChunkIterator(TypedComponentArray<T>* arr, size_t chunkSz)
                : array(arr), chunkSize(chunkSz), currentChunk(0)
            {
                totalChunks = array->GetChunkCount(chunkSize);
            }
            
            bool HasNext() const
            {
                return currentChunk < totalChunks;
            }
            
            std::pair<std::vector<T*>, std::vector<EntityID>> GetNext()
            {
                if (!HasNext())
                {
                    return {{}, {}};
                }
                
                size_t chunkStart = currentChunk * chunkSize;
                auto components = array->GetChunk(chunkStart, chunkSize);
                auto entities = array->GetChunkEntityIDs(chunkStart, chunkSize);
                
                currentChunk++;
                return {std::move(components), std::move(entities)};
            }
        };
        
        // 청크 반복자 생성
        ChunkIterator CreateChunkIterator(size_t chunkSize)
        {
            return ChunkIterator(this, chunkSize);
        }

    private:
        std::vector<T> components;                          // Dense array of components (contiguous memory)
        std::vector<EntityID> entityIDs;                    // Entity ID for each component
        std::unordered_map<EntityID, size_t> entityToIndex; // Sparse map: entity -> index in dense array
    };
}
