#pragma once

#include <string>
#include <utility>
#include <expected>

namespace Engine
{
    // ========================================
    // Engine Error Codes
    // ========================================
    
    enum class EngineErrorCode
    {
        Success = 0,
        
        // Initialization & Lifecycle errors
        AlreadyInitialized,
        NotInitialized,
        PlatformInitFailed,
        WindowCreationFailed,
        SubsystemInitFailed,
        ShutdownFailed,
        
        // Memory & Resource errors
        MemoryAllocationFailed,
        ResourceNotFound,
        ResourceLoadFailed,
        InvalidResourceHandle,
        
        // Job System errors
        InvalidJobHandle,
        CircularDependency,
        JobExecutionFailed,
        
        // ECS errors
        EntityNotFound,
        ComponentNotFound,
        InvalidComponentType,
        SystemRegistrationFailed,
        
        // Renderer errors
        ShaderCompilationFailed,
        PipelineCreationFailed,
        RenderPassFailed,
        InvalidBatchKey,
        
        // Asset errors
        AssetNotFound,
        AssetAlreadyRegistered,
        AssetImportFailed,
        UnsupportedAssetType,
        FileNotFound,
        CacheNotAvailable,
        
        // General Runtime errors
        InvalidState,
        InvalidParameter,
        OperationFailed,
        NotImplemented
    };
    
    // ========================================
    // Engine Error Structure
    // ========================================
    
    struct EngineError
    {
        EngineErrorCode code;
        std::string message;
        std::string component;
        
        EngineError(EngineErrorCode code, std::string message = "", std::string component = "Core")
            : code(code), message(std::move(message)), component(std::move(component)) {}

        template<typename T = void>
        static inline std::unexpected<EngineError> MakeError(EngineErrorCode code, std::string message = "", std::string component = "Core")
        {
            return std::unexpected<EngineError>(EngineError(code, std::move(message), std::move(component)));
        }
    };

    // ========================================
    // Standard Result Type (C++23 std::expected)
    // ========================================
    
    template <typename T>
    using Result = std::expected<T, EngineError>;

    // Helper to easily return an unexpected error
    inline std::unexpected<EngineError> MakeUnexpected(EngineErrorCode code, std::string message = "", std::string component = "Core")
    {
        return std::unexpected<EngineError>(EngineError(code, std::move(message), std::move(component)));
    }

    // Helper to easily return an error from a Result<T> function
    template<typename T = void>
    inline std::unexpected<EngineError> MakeError(EngineErrorCode code, std::string message = "", std::string component = "Core")
    {
        return std::unexpected<EngineError>(EngineError(code, std::move(message), std::move(component)));
    }

} // namespace Engine
