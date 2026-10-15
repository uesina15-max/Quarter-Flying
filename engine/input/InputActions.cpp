#include "InputActions.h"
#include "../core/Utf8Path.h"
#include "../core/logging/Logger.h"
#include <fmt/format.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

namespace Engine
{
    namespace
    {
        constexpr const char* kComponent = "InputActions";

        // KeyCode 이름 표. platform/IPlatform.h의 enum 이름과 같다(viewport.py의 키 표, 바인딩의 KeyCode와도 같다).
        // enum에 키를 추가하면 여기에도 추가한다 - 빠지면 그 키 이름은 "모르는 입력"으로 경고되고 건너뛴다.
        const std::vector<std::pair<std::string_view, KeyCode>>& KeyNameTable()
        {
            static const std::vector<std::pair<std::string_view, KeyCode>> table = [] {
                std::vector<std::pair<std::string_view, KeyCode>> t;
                static const char* letters[] = { "A","B","C","D","E","F","G","H","I","J","K","L","M",
                                                 "N","O","P","Q","R","S","T","U","V","W","X","Y","Z" };
                for (int i = 0; i < 26; ++i) t.emplace_back(letters[i], static_cast<KeyCode>('A' + i));
                static const char* digits[] = { "Num0","Num1","Num2","Num3","Num4","Num5","Num6","Num7","Num8","Num9" };
                for (int i = 0; i < 10; ++i) t.emplace_back(digits[i], static_cast<KeyCode>('0' + i));
                static const char* fkeys[] = { "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12" };
                for (int i = 0; i < 12; ++i)
                    t.emplace_back(fkeys[i], static_cast<KeyCode>(static_cast<uint16_t>(KeyCode::F1) + i));
                t.emplace_back("Escape", KeyCode::Escape);
                t.emplace_back("Space", KeyCode::Space);
                t.emplace_back("Enter", KeyCode::Enter);
                t.emplace_back("Tab", KeyCode::Tab);
                t.emplace_back("Backspace", KeyCode::Backspace);
                t.emplace_back("Shift", KeyCode::Shift);
                t.emplace_back("Control", KeyCode::Control);
                t.emplace_back("Alt", KeyCode::Alt);
                t.emplace_back("Left", KeyCode::Left);
                t.emplace_back("Up", KeyCode::Up);
                t.emplace_back("Right", KeyCode::Right);
                t.emplace_back("Down", KeyCode::Down);
                return t;
            }();
            return table;
        }

        constexpr std::array<std::pair<std::string_view, MouseButton>, 3> kMouseNames = {{
            { "Left", MouseButton::Left }, { "Right", MouseButton::Right }, { "Middle", MouseButton::Middle } }};

        constexpr std::array<std::pair<std::string_view, AnalogSource>, 3> kAnalogNames = {{
            { "MouseDeltaX", AnalogSource::MouseDeltaX }, { "MouseDeltaY", AnalogSource::MouseDeltaY },
            { "Wheel", AnalogSource::Wheel } }};

        std::optional<InputSource> ParseSource(std::string_view text)
        {
            constexpr std::string_view keyPrefix = "Key:";
            constexpr std::string_view mousePrefix = "Mouse:";
            if (text.starts_with(keyPrefix))
            {
                const std::string_view name = text.substr(keyPrefix.size());
                for (const auto& [n, code] : KeyNameTable())
                {
                    if (n == name) return InputSource{ InputSource::Kind::Key, code, MouseButton::Left };
                }
                return std::nullopt;
            }
            if (text.starts_with(mousePrefix))
            {
                const std::string_view name = text.substr(mousePrefix.size());
                for (const auto& [n, button] : kMouseNames)
                {
                    if (n == name) return InputSource{ InputSource::Kind::MouseButton, KeyCode::Unknown, button };
                }
            }
            return std::nullopt;
        }

        std::string SourceName(const InputSource& source)
        {
            if (source.kind == InputSource::Kind::MouseButton)
            {
                for (const auto& [n, button] : kMouseNames)
                    if (button == source.button) return "Mouse:" + std::string(n);
                return "Mouse:?";
            }
            for (const auto& [n, code] : KeyNameTable())
                if (code == source.key) return "Key:" + std::string(n);
            return "Key:?";
        }

        std::string_view AnalogName(AnalogSource source)
        {
            for (const auto& [n, s] : kAnalogNames)
                if (s == source) return n;
            return "?";
        }

        bool IsSourceDown(const InputState& input, const InputSource& s)
        {
            return s.kind == InputSource::Kind::Key ? input.IsKeyDown(s.key) : input.IsMouseButtonDown(s.button);
        }

        bool WasSourcePressed(const InputState& input, const InputSource& s)
        {
            return s.kind == InputSource::Kind::Key ? input.WasKeyPressed(s.key) : input.WasMouseButtonPressed(s.button);
        }

        float AnalogValue(const InputState& input, AnalogSource s)
        {
            switch (s)
            {
            case AnalogSource::MouseDeltaX: return input.GetMouseDeltaX();
            case AnalogSource::MouseDeltaY: return input.GetMouseDeltaY();
            case AnalogSource::Wheel:       return input.GetWheelDelta();
            }
            return 0.0f;
        }

        // 바인딩 배열을 액션 타입에 맞게 읽는다. 모양이 틀린 바인딩은 경고하고 그것만 건너뛴다.
        std::vector<InputBinding> ParseBindings(const std::string& action, InputActionType type,
                                                const nlohmann::json& bindings, std::vector<std::string>& warnings)
        {
            std::vector<InputBinding> out;
            for (size_t i = 0; i < bindings.size(); ++i)
            {
                const nlohmann::json& b = bindings[i];
                const std::string where = fmt::format("action '{}' binding #{}", action, i);
                if (type == InputActionType::Button)
                {
                    if (!b.is_string())
                    {
                        warnings.push_back(where + ": a button binding must be a string like \"Key:Space\"; skipped");
                        continue;
                    }
                    auto source = ParseSource(b.get<std::string>());
                    if (!source)
                    {
                        warnings.push_back(fmt::format("{}: unknown input '{}'; skipped", where, b.get<std::string>()));
                        continue;
                    }
                    InputBinding binding;
                    binding.kind = InputBinding::Kind::Digital;
                    binding.input = *source;
                    out.push_back(binding);
                    continue;
                }

                // axis
                if (!b.is_object())
                {
                    warnings.push_back(where + ": an axis binding must be an object "
                                               "({\"negative\",\"positive\"} or {\"source\",\"scale\"}); skipped");
                    continue;
                }
                if (b.contains("source"))
                {
                    if (!b["source"].is_string())
                    {
                        warnings.push_back(where + ": 'source' must be a string; skipped");
                        continue;
                    }
                    const std::string name = b["source"].get<std::string>();
                    std::optional<AnalogSource> analog;
                    for (const auto& [n, s] : kAnalogNames)
                        if (n == name) analog = s;
                    if (!analog)
                    {
                        warnings.push_back(fmt::format("{}: unknown analog source '{}' (MouseDeltaX, MouseDeltaY, Wheel); skipped",
                                                       where, name));
                        continue;
                    }
                    float scale = 1.0f;
                    if (b.contains("scale"))
                    {
                        if (!b["scale"].is_number())
                        {
                            warnings.push_back(where + ": 'scale' must be a number; skipped");
                            continue;
                        }
                        scale = b["scale"].get<float>();
                    }
                    InputBinding binding;
                    binding.kind = InputBinding::Kind::Analog;
                    binding.analog = *analog;
                    binding.scale = scale;
                    out.push_back(binding);
                    continue;
                }
                if (!b.contains("negative") || !b.contains("positive") ||
                    !b["negative"].is_string() || !b["positive"].is_string())
                {
                    warnings.push_back(where + ": an axis pair needs string 'negative' and 'positive'; skipped");
                    continue;
                }
                auto neg = ParseSource(b["negative"].get<std::string>());
                auto pos = ParseSource(b["positive"].get<std::string>());
                if (!neg || !pos)
                {
                    warnings.push_back(fmt::format("{}: unknown input '{}'; skipped", where,
                                                   !neg ? b["negative"].get<std::string>() : b["positive"].get<std::string>()));
                    continue;
                }
                InputBinding binding;
                binding.kind = InputBinding::Kind::Pair;
                binding.negative = *neg;
                binding.positive = *pos;
                out.push_back(binding);
            }
            return out;
        }

        // actions 항목 하나. 타입·bindings 모양이 틀리면 nullopt(그 액션을 건너뜀) + 경고.
        std::optional<InputActionDefinition> ParseAction(const std::string& name, const nlohmann::json& def,
                                                         std::vector<std::string>& warnings)
        {
            if (name.empty())
            {
                warnings.push_back("an action with an empty name; skipped");
                return std::nullopt;
            }
            if (!def.is_object() || !def.contains("type") || !def["type"].is_string())
            {
                warnings.push_back(fmt::format("action '{}': needs a string 'type' (\"button\" or \"axis\"); skipped", name));
                return std::nullopt;
            }
            const std::string type = def["type"].get<std::string>();
            InputActionDefinition out;
            if (type == "button") out.type = InputActionType::Button;
            else if (type == "axis") out.type = InputActionType::Axis;
            else
            {
                warnings.push_back(fmt::format("action '{}': unknown type '{}' (\"button\" or \"axis\"); skipped", name, type));
                return std::nullopt;
            }
            if (!def.contains("bindings") || !def["bindings"].is_array())
            {
                warnings.push_back(fmt::format("action '{}': 'bindings' must be an array; skipped", name));
                return std::nullopt;
            }
            out.bindings = ParseBindings(name, out.type, def["bindings"], warnings);
            return out;
        }

        nlohmann::json BindingToJson(const InputBinding& b)
        {
            switch (b.kind)
            {
            case InputBinding::Kind::Digital:
                return SourceName(b.input);
            case InputBinding::Kind::Pair:
                return nlohmann::json{ { "negative", SourceName(b.negative) }, { "positive", SourceName(b.positive) } };
            case InputBinding::Kind::Analog:
                return nlohmann::json{ { "source", std::string(AnalogName(b.analog)) }, { "scale", b.scale } };
            }
            return nullptr;
        }

        void LogWarnings(const std::string& sourceLabel, const std::vector<std::string>& warnings)
        {
            for (const auto& w : warnings)
                Logger::Warning("InputActions: {}: {}", sourceLabel, w);
        }
    }

    // ── 불러오기·저장 ────────────────────────────────────────────────────────

    Result<InputActionsLoadReport> InputActions::LoadFromJson(const nlohmann::json& root, const std::string& sourceLabel)
    {
        if (!root.is_object())
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter,
                                  fmt::format("{}: top level must be an object", sourceLabel), kComponent);
        }
        if (!root.contains("format") || !root["format"].is_string() || root["format"].get<std::string>() != kFormat)
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter,
                                  fmt::format("{}: 'format' must be \"{}\" (not an input actions file?)", sourceLabel, kFormat),
                                  kComponent);
        }
        if (!root.contains("version") || !root["version"].is_number_integer())
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter,
                                  fmt::format("{}: needs an integer 'version'", sourceLabel), kComponent);
        }
        const int version = root["version"].get<int>();
        if (version > kVersion)
        {
            return MakeUnexpected(EngineErrorCode::NotSupported,
                                  fmt::format("{}: version {} is newer than this engine supports ({})",
                                              sourceLabel, version, kVersion), kComponent);
        }

        InputActionsLoadReport report;
        std::map<std::string, ActionState> parsed;
        if (!root.contains("actions"))
        {
            report.warnings.push_back("no 'actions' object; starting with an empty map");
        }
        else if (!root["actions"].is_object())
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter,
                                  fmt::format("{}: 'actions' must be an object", sourceLabel), kComponent);
        }
        else
        {
            for (const auto& [name, def] : root["actions"].items())
            {
                if (auto action = ParseAction(name, def, report.warnings))
                {
                    ActionState state;
                    state.def = std::move(*action);
                    parsed.emplace(name, std::move(state));
                }
            }
        }

        // 여기까지 오면 거절할 일이 없다 - 이제야 기존 맵을 바꾼다(실패 시 기존 맵 유지).
        actions_ = std::move(parsed);
        warned_.clear();
        report.actionCount = actions_.size();
        LogWarnings(sourceLabel, report.warnings);
        return report;
    }

    Result<InputActionsLoadReport> InputActions::LoadActions(const std::string& utf8Path)
    {
        const std::filesystem::path path = PathFromUtf8(utf8Path);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
        {
            actions_.clear();
            warned_.clear();
            InputActionsLoadReport report;
            report.fileMissing = true;
            report.warnings.push_back("file not found; starting with an empty action map");
            LogWarnings(utf8Path, report.warnings);
            return report;
        }
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            return MakeUnexpected(EngineErrorCode::ResourceLoadFailed, fmt::format("cannot read '{}'", utf8Path), kComponent);
        }
        nlohmann::json root;
        try
        {
            in >> root;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            return MakeUnexpected(EngineErrorCode::ResourceLoadFailed,
                                  fmt::format("JSON syntax error in '{}': {}", utf8Path, e.what()), kComponent);
        }
        return LoadFromJson(root, utf8Path);
    }

    nlohmann::json InputActions::ToJson() const
    {
        nlohmann::json actions = nlohmann::json::object();
        for (const auto& [name, state] : actions_)
        {
            nlohmann::json bindings = nlohmann::json::array();
            for (const auto& b : state.def.bindings) bindings.push_back(BindingToJson(b));
            actions[name] = { { "type", state.def.type == InputActionType::Button ? "button" : "axis" },
                              { "bindings", std::move(bindings) } };
        }
        return { { "format", kFormat }, { "version", kVersion }, { "actions", std::move(actions) } };
    }

    Result<void> InputActions::SaveActions(const std::string& utf8Path) const
    {
        const std::filesystem::path path = PathFromUtf8(utf8Path);
        std::error_code ec;
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path(), ec);
            if (ec)
            {
                return MakeUnexpected(EngineErrorCode::OperationFailed,
                                      fmt::format("cannot create folder for '{}': {}", utf8Path, ec.message()), kComponent);
            }
        }
        // 임시 파일에 다 쓴 뒤 바꿔치기한다. 쓰는 도중 실패해도 기존 파일이 반쯤 쓰인 채 남지 않는다.
        std::filesystem::path tmp = path;
        tmp += ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                return MakeUnexpected(EngineErrorCode::OperationFailed, fmt::format("cannot write '{}'", utf8Path), kComponent);
            }
            out << ToJson().dump(2) << '\n';
            if (!out)
            {
                return MakeUnexpected(EngineErrorCode::OperationFailed, fmt::format("write failed for '{}'", utf8Path), kComponent);
            }
        }
        std::filesystem::rename(tmp, path, ec);
        if (ec)
        {
            std::filesystem::remove(tmp, ec);
            return MakeUnexpected(EngineErrorCode::OperationFailed,
                                  fmt::format("cannot replace '{}'", utf8Path), kComponent);
        }
        return {};
    }

    // ── 편집 ─────────────────────────────────────────────────────────────────

    Result<std::vector<std::string>> InputActions::SetActionJson(const std::string& name, const nlohmann::json& def)
    {
        std::vector<std::string> warnings;
        auto action = ParseAction(name, def, warnings);
        if (!action)
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter,
                                  warnings.empty() ? fmt::format("invalid action '{}'", name) : warnings.back(), kComponent);
        }
        ActionState state;
        state.def = std::move(*action);
        actions_[name] = std::move(state);
        warned_.clear();
        LogWarnings("<edit>", warnings);
        return warnings;
    }

    Result<std::vector<std::string>> InputActions::Rebind(const std::string& name, const nlohmann::json& bindings)
    {
        auto it = actions_.find(name);
        if (it == actions_.end())
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter, fmt::format("no action named '{}'", name), kComponent);
        }
        if (!bindings.is_array())
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter,
                                  fmt::format("action '{}': bindings must be an array", name), kComponent);
        }
        std::vector<std::string> warnings;
        it->second.def.bindings = ParseBindings(name, it->second.def.type, bindings, warnings);
        LogWarnings("<rebind>", warnings);
        return warnings;
    }

    bool InputActions::RemoveAction(const std::string& name)
    {
        return actions_.erase(name) > 0;
    }

    void InputActions::Clear()
    {
        actions_.clear();
        warned_.clear();
    }

    std::vector<std::string> InputActions::GetActionNames() const
    {
        std::vector<std::string> names;
        names.reserve(actions_.size());
        for (const auto& [name, state] : actions_) names.push_back(name);
        return names;
    }

    bool InputActions::HasAction(const std::string& name) const
    {
        return actions_.contains(name);
    }

    // ── 프레임 갱신 ──────────────────────────────────────────────────────────

    void InputActions::Update(const InputState& input)
    {
        for (auto& [name, state] : actions_)
        {
            if (state.def.type == InputActionType::Button)
            {
                const bool wasDown = state.down;   // 직전 프레임 값
                bool down = false;
                bool pressedEdge = false;
                for (const auto& b : state.def.bindings)
                {
                    if (b.kind != InputBinding::Kind::Digital) continue;
                    down = down || IsSourceDown(input, b.input);
                    pressedEdge = pressedEdge || WasSourcePressed(input, b.input);
                }
                state.pressed = !wasDown && pressedEdge;
                state.released = (wasDown && !down) || (state.pressed && !down);
                state.down = down;
            }
            else
            {
                float digital = 0.0f;
                float analog = 0.0f;
                for (const auto& b : state.def.bindings)
                {
                    if (b.kind == InputBinding::Kind::Pair)
                    {
                        digital += (IsSourceDown(input, b.positive) ? 1.0f : 0.0f)
                                 - (IsSourceDown(input, b.negative) ? 1.0f : 0.0f);
                    }
                    else if (b.kind == InputBinding::Kind::Analog)
                    {
                        analog += AnalogValue(input, b.analog) * b.scale;
                    }
                }
                state.axis = std::clamp(digital, -1.0f, 1.0f) + analog;
            }
        }
    }

    void InputActions::ResetHeldState()
    {
        for (auto& [name, state] : actions_)
        {
            state.down = false;
            state.pressed = false;
            state.released = false;
            state.axis = 0.0f;
        }
    }

    // ── 현재 프레임의 액션 맵 ────────────────────────────────────────────────

    namespace
    {
        thread_local InputActions* t_current = nullptr;
    }

    InputActions* InputActions::Current()
    {
        return t_current;
    }

    InputActions::ScopedCurrent::ScopedCurrent(InputActions* actions)
        : previous_(t_current)
    {
        t_current = actions;
    }

    InputActions::ScopedCurrent::~ScopedCurrent()
    {
        t_current = previous_;
    }

    // ── 읽기 ─────────────────────────────────────────────────────────────────

    void InputActions::WarnOnce(const std::string& key, const std::string& message) const
    {
        if (warned_.insert(key).second)
        {
            Logger::Warning("InputActions: {}", message);
        }
    }

    const InputActions::ActionState* InputActions::Find(const std::string& name, InputActionType expected,
                                                        const char* api) const
    {
        auto it = actions_.find(name);
        if (it == actions_.end())
        {
            WarnOnce("missing:" + name, fmt::format("{}('{}'): no such action (returning 0/false). "
                                                    "Check input/actions.json", api, name));
            return nullptr;
        }
        if (it->second.def.type != expected)
        {
            WarnOnce(fmt::format("type:{}:{}", api, name),
                     fmt::format("{}('{}'): action is {} (returning 0/false)", api, name,
                                 it->second.def.type == InputActionType::Button ? "a button - use IsDown/WasPressed/WasReleased"
                                                                                : "an axis - use GetAxis"));
            return nullptr;
        }
        return &it->second;
    }

    float InputActions::GetAxis(const std::string& name) const
    {
        const ActionState* s = Find(name, InputActionType::Axis, "GetAxis");
        return s ? s->axis : 0.0f;
    }

    bool InputActions::IsDown(const std::string& name) const
    {
        const ActionState* s = Find(name, InputActionType::Button, "IsDown");
        return s && s->down;
    }

    bool InputActions::WasPressed(const std::string& name) const
    {
        const ActionState* s = Find(name, InputActionType::Button, "WasPressed");
        return s && s->pressed;
    }

    bool InputActions::WasReleased(const std::string& name) const
    {
        const ActionState* s = Find(name, InputActionType::Button, "WasReleased");
        return s && s->released;
    }
}
