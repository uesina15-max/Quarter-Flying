#include "ScriptSystem.h"
#include "ScriptComponent.h"
#include <string>

namespace pybind11 { class object; }

#include "ECSRegistry.h"
#include "ComponentManager.h"
#include "ComponentArray.h"
#include "../core/logging/Logger.h"
#include <fmt/format.h>
#include <pybind11/pybind11.h>
#include <pybind11/eval.h>

namespace py = pybind11;

namespace Engine
{
    void ScriptSystem::ClearComponentCache(ScriptComponent& comp)
    {
        py::gil_scoped_acquire acquire; // 안전을 위해 GIL 획득
        if (comp.cachedOnUpdate) {
            delete static_cast<py::object*>(comp.cachedOnUpdate);
            comp.cachedOnUpdate = nullptr;
        }
        if (comp.cachedOnDestroy) {
            delete static_cast<py::object*>(comp.cachedOnDestroy);
            comp.cachedOnDestroy = nullptr;
        }
        if (comp.pyInstance) {
            delete static_cast<py::object*>(comp.pyInstance);
            comp.pyInstance = nullptr;
        }
        comp.hasUpdate = false;
        comp.hasDestroy = false;
    }

    void ScriptSystem::Initialize(ECSRegistry& registry)
    {
        Logger::Info("ScriptSystem Initialized");
    }

    void ScriptSystem::Shutdown(ECSRegistry& registry)
    {
        auto array = registry.GetComponentArray<ScriptComponent>();
        if (!array) return;
        
        for (size_t i = 0; i < array->Size(); ++i) {
            auto& comp = array->GetDenseArray()[i];
            ClearComponentCache(comp);
        }
    }

    void ScriptSystem::OnStart(ECSRegistry& registry)
    {
        // Optional: can be used to notify scripts right before the first tick
    }

    void ScriptSystem::OnStop(ECSRegistry& registry)
    {
        auto array = registry.GetComponentArray<ScriptComponent>();
        if (!array) return;

        for (size_t i = 0; i < array->Size(); ++i) {
            auto& comp = array->GetDenseArray()[i];
            if (comp.hasDestroy && comp.cachedOnDestroy) {
                try {
                    py::object* destroyFunc = static_cast<py::object*>(comp.cachedOnDestroy);
                    (*destroyFunc)();
                } catch (const std::exception& e) {
                    Logger::Error("ScriptSystem OnStop Error: {}", e.what());
                }
            } else if (comp.pyInstance) {
                py::object* obj = static_cast<py::object*>(comp.pyInstance);
                try {
                    if (py::hasattr(*obj, "on_destroy")) {
                        obj->attr("on_destroy")();
                    }
                } catch (const std::exception& e) {
                    Logger::Error("ScriptSystem OnStop Error: {}", e.what());
                }
            }
            ClearComponentCache(comp);
            // Stop 이후 롤백된 상태에서 다음 Play 시작 시 새 인스턴스로 부활하도록.
            comp.dirty = true;
        }
    }

    void ScriptSystem::Update(ECSRegistry& registry, float deltaTime)
    {
        auto array = registry.GetComponentArray<ScriptComponent>();
        if (!array) return;

        for (size_t i = 0; i < array->Size(); ++i) {
            auto& comp = array->GetDenseArray()[i];
            EntityID entityId = array->GetEntityIDs()[i];
            
            // 1. Hot Reload / Instantiate via dirty flag
            if (!comp.pyInstance || comp.dirty) {
                ClearComponentCache(comp);
                
                comp.dirty = false;
                
                if (!comp.moduleName.empty() && !comp.className.empty()) {
                    try {
                        py::module_ mod = py::module_::import(comp.moduleName.c_str());
                        mod.reload(); // 스크립트 모듈 Hot Reload
                        
                        py::object pyClass = mod.attr(comp.className.c_str());
                        // 스크립트 객체화 및 Entity 주입
                        py::object instance = pyClass(Entity(entityId));
                        
                        comp.pyInstance = new py::object(instance);
                        
                        // 핸들 캐싱: 문자열 룩업을 여기서 미리 수행
                        if (py::hasattr(instance, "on_update")) {
                            comp.cachedOnUpdate = new py::object(instance.attr("on_update"));
                            comp.hasUpdate = true;
                        } else {
                            comp.cachedOnUpdate = nullptr;
                            comp.hasUpdate = false;
                        }

                        if (py::hasattr(instance, "on_destroy")) {
                            comp.cachedOnDestroy = new py::object(instance.attr("on_destroy"));
                            comp.hasDestroy = true;
                        } else {
                            comp.cachedOnDestroy = nullptr;
                            comp.hasDestroy = false;
                        }
                        
                        // Lifecycle: on_create
                        if (py::hasattr(instance, "on_create")) {
                            instance.attr("on_create")();
                        }
                        
                        std::string msg = fmt::format("ScriptSystem: Reloaded {}::{}", comp.moduleName, comp.className);
                        Logger::Info("{}", msg.c_str());
                    } catch (const std::exception& e) {
                        Logger::Error("ScriptSystem HotReload Error: {}", e.what());
                        ClearComponentCache(comp);
                    }
                }
            }

            // 2. Execution Guard & Validation (Hot Path Optimization)
            if (comp.hasUpdate && comp.cachedOnUpdate) {
                // 캐싱된 py::object를 함수처럼 직접 호출 (룩업 비용 및 try/catch 0)
                py::object* updateFunc = static_cast<py::object*>(comp.cachedOnUpdate);
                (*updateFunc)(deltaTime);
            }
        }
    }
}
