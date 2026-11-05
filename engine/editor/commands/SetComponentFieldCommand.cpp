#include "SetComponentFieldCommand.h"
#include "../../ecs/ECSRegistry.h"
#include "../../ecs/Reflection.h"
#include <algorithm>
#include <fmt/format.h>

namespace Engine
{

namespace
{
    constexpr const char* kSource = "SetComponentFieldCommand";
}

SetComponentFieldCommand::SetComponentFieldCommand(ECSRegistry* registry, Entity entity, std::string componentName,
                                                   std::string fieldName, nlohmann::json newValue,
                                                   uint32_t expectedVersion)
    : registry_(registry)
    , entity_(entity)
    , entityUUID_(registry && registry->IsValid(entity) ? registry->GetUUID(entity) : UUID(0))
    , componentName_(std::move(componentName))
    , fieldName_(std::move(fieldName))
    , newValue_(std::move(newValue))
    , expectedVersion_(expectedVersion)
{
    // 이전 값은 생성 시점(= 바꾸기 직전)에 떼어 둔다. 필드가 없거나 직렬화할 수 없으면 hasOldValue_가 false로
    // 남고, Apply가 그 이유를 오류로 돌려준다(되돌릴 수 없는 편집을 기록하지 않기 위해).
    const ComponentInfo* info = ComponentRegistry::GetComponentInfo(componentName_);
    if (!registry_ || !info || !info->serialize || !info->hasComponent || !entity_.IsValid()
        || !info->hasComponent(*registry_, entity_))
    {
        return;
    }
    nlohmann::json snapshot;
    info->serialize(*registry_, entity_, snapshot);
    if (snapshot.contains(componentName_) && snapshot[componentName_].contains(fieldName_))
    {
        oldValue_ = snapshot[componentName_][fieldName_];
        hasOldValue_ = true;
    }
}

std::expected<void, EngineError> SetComponentFieldCommand::Validate()
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "ECS registry not set", kSource);
    }
    // Resolve again after creation/deletion commands restore this entity.
    entity_ = registry_->GetEntityByUUID(entityUUID_);
    if (!entity_.IsValid() || !registry_->IsValid(entity_))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", kSource);
    }
    const ComponentInfo* info = ComponentRegistry::GetComponentInfo(componentName_);
    if (!info)
    {
        return MakeError(EngineErrorCode::InvalidComponentType,
                         fmt::format("'{}' is not a registered component", componentName_), kSource);
    }
    // SetComponentFieldJson과 같은 보호: Inspector가 다른 스키마 버전으로 만든 위젯의 값을 쓰지 않는다.
    if (info->version != expectedVersion_)
    {
        return MakeError(EngineErrorCode::InvalidState,
                         fmt::format("schema version mismatch for {}: expected {}, got {}", componentName_,
                                     info->version, expectedVersion_), kSource);
    }
    if (!info->patchField || !info->hasComponent)
    {
        return MakeError(EngineErrorCode::NotSupported,
                         fmt::format("'{}' does not support field edits", componentName_), kSource);
    }
    if (!info->hasComponent(*registry_, entity_))
    {
        return MakeError(EngineErrorCode::ComponentNotFound,
                         fmt::format("entity has no {}", componentName_), kSource);
    }
    // patchField는 모르는 필드 이름이면 아무 일도 하지 않고 조용히 끝난다. 그러면 명령은 "성공"으로 기록되는데
    // 값은 그대로라 Undo/변경 표시가 거짓말을 하게 된다 — 그래서 필드 이름을 먼저 확인한다.
    const bool known = std::any_of(info->fields.begin(), info->fields.end(),
                                   [&](const auto& f) { return f.name == fieldName_; });
    if (!known)
    {
        return MakeError(EngineErrorCode::InvalidPropertyPath,
                         fmt::format("{} has no field '{}'", componentName_, fieldName_), kSource);
    }
    return {};
}

std::expected<void, EngineError> SetComponentFieldCommand::Write(const nlohmann::json& value)
{
    if (auto v = Validate(); !v)
    {
        return v;
    }
    const ComponentInfo* info = ComponentRegistry::GetComponentInfo(componentName_);
    try
    {
        info->patchField(*registry_, entity_, StringHash(fieldName_.c_str()), value);
    }
    catch (const std::exception& e)
    {
        // 형식이 맞지 않는 값(예: Vec3 필드에 문자열) — nlohmann::json이 type_error를 던진다.
        return MakeError(EngineErrorCode::InvalidParameter,
                         fmt::format("{}.{}: invalid value {}: {}", componentName_, fieldName_, value.dump(), e.what()),
                         kSource);
    }
    return {};
}

std::expected<void, EngineError> SetComponentFieldCommand::Apply()
{
    if (!hasOldValue_)
    {
        // 이전 값을 못 떼어 냈다 = 컴포넌트·필드가 없거나 직렬화할 수 없다. Validate가 더 정확한 이유를 낸다.
        // (값을 바꾼 뒤에 실패를 돌려주면 기록되지 않은 변경이 남으므로 쓰지 않고 검사만 한다.)
        if (auto v = Validate(); !v)
        {
            return v;
        }
        return MakeError(EngineErrorCode::OperationFailed,
                         fmt::format("{}.{}: previous value could not be captured, edit not recorded",
                                     componentName_, fieldName_), kSource);
    }
    return Write(newValue_);
}

std::expected<void, EngineError> SetComponentFieldCommand::Undo()
{
    return Write(oldValue_);
}

bool SetComponentFieldCommand::CanMergeWith(const ICommand& other) const
{
    const auto* o = dynamic_cast<const SetComponentFieldCommand*>(&other);
    return o != nullptr && o->registry_ == registry_ && entityUUID_.IsValid() && o->entityUUID_ == entityUUID_ && o->componentName_ == componentName_
        && o->fieldName_ == fieldName_;
}

std::expected<void, EngineError> SetComponentFieldCommand::MergeWith(const ICommand& other)
{
    const auto* o = dynamic_cast<const SetComponentFieldCommand*>(&other);
    if (!o)
    {
        return MakeError(EngineErrorCode::OperationFailed, "Not mergeable", kSource);
    }
    newValue_ = o->newValue_;   // 이전 값은 첫 편집 직전 값 그대로 둔다(Undo 한 번에 편집 전으로)
    return {};
}

} // namespace Engine
