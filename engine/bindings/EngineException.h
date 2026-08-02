#pragma once

#include "../core/EngineError.h"
#include <stdexcept>
#include <expected>
#include <type_traits>

namespace Engine::Bindings
{

struct EngineException : std::runtime_error
{
    explicit EngineException(EngineError err)
        : std::runtime_error(err.message)
        , error(std::move(err))
    {
    }

    EngineError error;
};

template<typename T>
T UnwrapOrThrow(std::expected<T, EngineError> result)
{
    if (!result)
    {
        throw EngineException(result.error());
    }

    if constexpr (std::is_void_v<T>)
    {
        return;
    }
    else
    {
        return *result;
    }
}

} // namespace Engine::Bindings
