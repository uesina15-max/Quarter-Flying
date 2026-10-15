#pragma once

#include "Utf8Path.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace Engine
{
    // ========================================
    // 프로젝트 상대 경로 (docs/USABILITY_IMPROVEMENT_PLAN.md X-1a)
    // ========================================
    //
    // 컴포넌트에 저장하는 에셋 경로(PrefabInstanceComponent.prefabPath 등)는 **프로젝트 폴더 기준 상대 경로**가
    // 정규 형식이다. meshPath·texturePath와 같은 기준이다(엔진의 assetRoot = 프로젝트 폴더).
    // 예전에는 prefabPath가 절대 경로로 저장되어, 씬 파일에 이 PC의 경로가 들어가고 프로젝트를 옮기면 Revert가
    // "Prefab file not found"로 깨졌다(GAME_BUILD_PLAN.md §9.2).
    //
    // 경로를 해석·정규화하는 곳은 여기 두 함수뿐이다. 프리팹 명령, 씬 불러오기 업그레이드, 바인딩이 모두 이것을
    // 부른다 - 기준 폴더나 구분자 규칙이 호출부마다 달라지면 "어떤 경로는 되고 어떤 경로는 안 되는" 버그가 된다.
    // 문자열은 모두 UTF-8이다(core/Utf8Path.h).

    namespace Detail
    {
        inline std::filesystem::path ComparablePath(const std::filesystem::path& p)
        {
            // 존재하는 부분은 실제 대소문자·심볼릭 링크로 맞춘다(C:\ vs c:\ 같은 차이로 "프로젝트 밖"이 되지 않게).
            std::error_code ec;
            std::filesystem::path canonical = std::filesystem::weakly_canonical(p, ec);
            return ec ? p.lexically_normal() : canonical;
        }
    }

    // stored(절대 또는 상대)를 실제 파일 경로로 바꾼다. 상대 경로는 projectRoot 기준. projectRoot가 비어 있으면
    // 상대 경로를 그대로 돌려준다(현재 작업 폴더 기준이 되므로, 호출자는 root를 넘겨야 한다).
    inline std::filesystem::path ResolveProjectPath(std::string_view stored, std::string_view projectRoot)
    {
        const std::filesystem::path p = PathFromUtf8(stored);
        if (p.is_absolute() || projectRoot.empty())
        {
            return p.lexically_normal();
        }
        return (PathFromUtf8(projectRoot) / p).lexically_normal();
    }

    // stored를 저장할 형식으로 바꾼다. 프로젝트 안을 가리키는 절대 경로 -> "/" 구분 상대 경로.
    // 상대 경로 -> "/" 구분자로만 정리. 프로젝트 밖의 절대 경로나 projectRoot가 비어 있으면 그대로 둔다.
    // insideProject가 있으면 "프로젝트 안(또는 상대 경로)인가"를 알려 준다(밖이면 호출자가 경고한다).
    inline std::string NormalizeProjectPath(std::string_view stored, std::string_view projectRoot,
                                            bool* insideProject = nullptr)
    {
        if (insideProject) *insideProject = true;
        if (stored.empty()) return std::string(stored);
        const std::filesystem::path p = PathFromUtf8(stored);
        if (!p.is_absolute())
        {
            return GenericUtf8FromPath(p.lexically_normal());
        }
        if (projectRoot.empty())
        {
            return std::string(stored);
        }
        const std::filesystem::path root = Detail::ComparablePath(PathFromUtf8(projectRoot));
        const std::filesystem::path full = Detail::ComparablePath(p);
        const std::filesystem::path rel = full.lexically_relative(root);
        const bool inside = !rel.empty() && !rel.is_absolute() && *rel.begin() != "..";
        if (!inside)
        {
            if (insideProject) *insideProject = false;
            return std::string(stored);
        }
        return GenericUtf8FromPath(rel);
    }
}
