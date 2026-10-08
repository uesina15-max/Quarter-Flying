// 게임 실행 파일(Player) — 에디터 없이 게임을 실행한다. docs/GAME_BUILD_PLAN.md §3.5, Phase 2.
//
// Build Game은 이 실행 파일을 컴파일하지 않고 복사해 이름만 바꾼다(D1). 그래서 게임마다 달라지는
// 것은 전부 exe 옆의 파일(game.json, scenes/, assets/, scripts/)에서 읽는다.
//
// 인자:
//   --smoke-test N   N프레임을 그린 뒤 종료 코드 0으로 끝낸다(Build Game의 자체 검증, Phase 3·4).
//                    이 모드에서는 메시지 상자를 띄우지 않는다.
//   --game PATH      game.json 경로(기본: exe 옆의 game.json).
//
// 종료 코드: 0 정상, 1 설정/씬 오류, 2 Python 초기화 오류, 3 엔진 초기화 오류, 4 스크립트 부트스트랩 오류.

#include <pybind11/embed.h>

#include "GameConfig.h"
#include "../core/Engine.h"
#include "../core/logging/Logger.h"
#include "../ecs/World.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Components.h"
#include "../scene/SceneIO.h"

#if __has_include("qf_dev_python_home.h")
#include "qf_dev_python_home.h"   // 빌드 폴더에서 생성(CMakeLists.txt)
#endif

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace py = pybind11;
namespace fs = std::filesystem;

namespace
{
    enum ExitCode : int
    {
        kOk = 0,
        kConfigOrSceneError = 1,
        kPythonError = 2,
        kEngineError = 3,
        kBootstrapError = 4,
    };

    std::string ToUtf8(const fs::path& p)
    {
        const std::u8string s = p.u8string();
        return std::string(s.begin(), s.end());
    }

    fs::path FromUtf8(const std::string& s)
    {
        return fs::path(std::u8string(s.begin(), s.end()));
    }

    std::wstring WideFromUtf8(const std::string& s)
    {
        if (s.empty()) return {};
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring w(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
        return w;
    }

    fs::path ExecutableDir()
    {
        std::wstring buf(32768, L'\0');
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        buf.resize(n);
        return fs::path(buf).parent_path();
    }

    struct Options
    {
        int smokeTestFrames = 0;   // 0이면 일반 실행
        fs::path gameJson;
    };

    // 실행 실패를 로그에 남기고, 사람이 실행한 경우(스모크 테스트가 아닐 때)에는 메시지 상자로도 알린다.
    // 더블클릭으로 실행한 게임이 아무 표시 없이 꺼지면 원인을 알 방법이 없기 때문이다.
    int Fatal(const Options& opt, int code, const std::string& message)
    {
        Engine::Logger::Fatal("Player - {}", message);
        if (opt.smokeTestFrames <= 0)
        {
            MessageBoxW(nullptr, WideFromUtf8(message).c_str(), L"Game failed to start", MB_OK | MB_ICONERROR);
        }
        return code;
    }

    bool ParseArgs(int argc, char** argv, const fs::path& exeDir, Options& opt, std::string& error)
    {
        opt.gameJson = exeDir / "game.json";
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--smoke-test" && i + 1 < argc)
            {
                try { opt.smokeTestFrames = std::max(1, std::stoi(argv[++i])); }
                catch (...) { error = "--smoke-test needs a frame count"; return false; }
            }
            else if (arg == "--game" && i + 1 < argc)
            {
                opt.gameJson = FromUtf8(argv[++i]);
            }
            else
            {
                error = "unknown argument: " + arg;
                return false;
            }
        }
        return true;
    }

    // ._pth 파일이 있으면(Build Game이 조립한 폴더) Python은 그 파일의 경로 목록만 쓴다(격리 모드).
    // 없으면 개발 빌드(build/Release)에서 실행한 것이므로, 빌드에 쓰인 Python 설치를 home으로 쓴다.
    // 개발 실행에서도 site-packages(PySide6 등)는 읽지 않는다 — 배포본과 같은 조건에서 시험하기 위해서다.
    void ConfigurePython(PyConfig& config, const fs::path& exeDir)
    {
        PyConfig_InitPythonConfig(&config);
        config.parse_argv = 0;
        config.install_signal_handlers = 0;
        config.site_import = 0;
        const bool shipped = fs::exists(exeDir / "python314._pth");
#ifdef QF_DEV_PYTHON_HOME
        if (!shipped)
        {
            PyConfig_SetString(&config, &config.home, QF_DEV_PYTHON_HOME);
        }
#else
        (void)shipped;
#endif
    }

    void PrependSysPath(const std::vector<fs::path>& dirs)
    {
        py::list path = py::module_::import("sys").attr("path");
        for (auto it = dirs.rbegin(); it != dirs.rend(); ++it)
        {
            const std::string s = ToUtf8(*it);
            bool present = false;
            for (const auto& item : path)
            {
                if (py::str(item).cast<std::string>() == s) { present = true; break; }
            }
            if (!present) path.insert(0, s);
        }
    }

    bool HasMainCamera(Engine::ECSRegistry& registry)
    {
        for (Engine::Entity e : registry.GetAllEntities())
        {
            if (const auto* cam = registry.GetComponent<Engine::CameraComponent>(e))
            {
                if (cam->isMainCamera) return true;
            }
        }
        return false;
    }
}

int main(int argc, char** argv)
{
    const fs::path exeDir = ExecutableDir();
    // 더블클릭·바로가기·다른 폴더에서 실행해도 상대 경로가 같은 곳을 가리키게 한다.
    std::error_code ec;
    fs::current_path(exeDir, ec);

    Engine::Logger::SetMinLevel(Engine::LogLevel::Info);

    Options opt;
    std::string argError;
    if (!ParseArgs(argc, argv, exeDir, opt, argError))
    {
        return Fatal(opt, kConfigOrSceneError, argError);
    }

    // 1. game.json
    auto cfg = Engine::LoadGameConfig(ToUtf8(opt.gameJson));
    if (!cfg)
    {
        return Fatal(opt, kConfigOrSceneError, cfg.error().message);
    }
    const fs::path gameDir = opt.gameJson.parent_path();
    const fs::path assetRoot = (gameDir / FromUtf8(cfg->assetRoot)).lexically_normal();
    const fs::path startScene = (gameDir / FromUtf8(cfg->startScene)).lexically_normal();

    // 2. Python
    PyConfig pyConfig;
    ConfigurePython(pyConfig, exeDir);
    std::unique_ptr<py::scoped_interpreter> interpreter;
    try
    {
        interpreter = std::make_unique<py::scoped_interpreter>(&pyConfig, 0, nullptr, false);
        // exe 폴더(quarterflying.pyd), python/(runtime_bootstrap 등), scripts/(게임 스크립트).
        PrependSysPath({ exeDir, exeDir / "python", gameDir / "scripts" });
        // 엔진 타입(Entity, ECSRegistry 등)을 pybind11에 등록한다. 이게 없으면 ScriptSystem이 스크립트
        // 생성자에 Entity를 넘길 때 "unregistered type"으로 실패한다.
        py::module_::import("quarterflying");
    }
    catch (const std::exception& e)
    {
        return Fatal(opt, kPythonError, std::string("Python initialization failed: ") + e.what());
    }

    int exitCode = kOk;
    {
        // 3. 엔진 (자체 Win32 창 + GL 컨텍스트)
        Engine::Engine engine;
        Engine::EngineConfig engineConfig;
        engineConfig.windowTitle = cfg->title;
        engineConfig.windowWidth = cfg->windowWidth;
        engineConfig.windowHeight = cfg->windowHeight;
        engineConfig.windowFullscreen = cfg->fullscreen;
        engineConfig.numWorkerThreads = cfg->numWorkerThreads;
        engineConfig.logFrameInterval = cfg->logFrameInterval;
        engineConfig.assetRoot = ToUtf8(assetRoot);
        engineConfig.showDebugGrid = false;     // 그리드는 뷰 카메라와 무관하게 그려진다(§3.5)
        engineConfig.scriptHotReload = false;

        auto init = engine.Initialize(engineConfig);
        if (!init)
        {
            return Fatal(opt, kEngineError, "engine initialization failed: " + init.error().message);
        }

        Engine::World* world = engine.CreateWorld(cfg->title);
        if (!world)
        {
            engine.Shutdown();
            return Fatal(opt, kEngineError, "could not create the game world");
        }
        engine.SetActiveWorld(world);

        // 4. 시작 씬
        auto loaded = Engine::LoadSceneFromFile(*world->GetRegistry(), ToUtf8(startScene));
        if (!loaded)
        {
            engine.Shutdown();
            return Fatal(opt, kConfigOrSceneError, "cannot load start scene: " + loaded.error().message);
        }
        if (!HasMainCamera(*world->GetRegistry()))
        {
            // 조용한 검은 화면을 막는다. 실행은 계속한다(기본 카메라 위치로는 그려진다).
            Engine::Logger::Error("Player - the start scene has no entity with CameraComponent.isMainCamera = true; "
                                  "the game view will use the default camera");
        }

        // 5. 게임 시작
        world->Play();

        // 6. Python 쪽 런타임(액션 재생 등). 모듈이 없으면 경고만 하고 계속한다.
        py::object tick;
        try
        {
            py::module_ bootstrap = py::module_::import("runtime_bootstrap");
            // ActionPlayerComponent.action의 상대 경로 기준은 에디터와 같이 "<에셋 기준 폴더>/assets"다
            // (에디터 main.py _on_play가 engine/assets를 넘기는 것과 같은 규칙).
            bootstrap.attr("start")(py::cast(world->GetRegistry(), py::return_value_policy::reference),
                                    ToUtf8(assetRoot / "assets"));
            tick = bootstrap.attr("tick");
        }
        catch (const py::error_already_set& e)
        {
            if (e.matches(PyExc_ModuleNotFoundError))
            {
                Engine::Logger::Warning("Player - runtime_bootstrap module not found; action playback is disabled");
            }
            else
            {
                Engine::Logger::Error("Player - runtime_bootstrap.start failed: {}", e.what());
                exitCode = kBootstrapError;
            }
        }

        // 7. 메인 루프
        int frames = 0;
        engine.Run([&](float dt) -> bool {
            if (tick)
            {
                try
                {
                    tick(dt);
                }
                catch (const py::error_already_set& e)
                {
                    Engine::Logger::Error("Player - runtime_bootstrap.tick failed; disabling it: {}", e.what());
                    tick = py::object();
                    exitCode = kBootstrapError;
                }
            }
            ++frames;
            return opt.smokeTestFrames <= 0 || frames < opt.smokeTestFrames;
        });

        // 8. 종료: Stop이 스크립트의 on_destroy를 부른다(ScriptSystem::OnStop). Shutdown 전에 해야 한다.
        tick = py::object();
        world->Stop();
        engine.Shutdown();

        if (opt.smokeTestFrames > 0)
        {
            Engine::Logger::Info("Player - smoke test finished: {} frames", frames);
            if (frames < opt.smokeTestFrames && exitCode == kOk)
            {
                // 창이 먼저 닫혔다(또는 엔진이 스스로 멈췄다).
                exitCode = kEngineError;
            }
        }
    }

    interpreter.reset();
    return exitCode;
}
