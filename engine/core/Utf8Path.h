#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Engine
{
    // ========================================
    // UTF-8 문자열 <-> std::filesystem::path
    // ========================================
    //
    // 엔진이 주고받는 경로 문자열(std::string)은 UTF-8이다(Python에서 넘어오는 str, JSON에 저장된 경로,
    // /utf-8로 컴파일한 소스의 리터럴). 그런데 Windows에서
    //   - std::filesystem::path(std::string)은 문자열을 **ANSI 코드페이지**로 해석하고,
    //   - path.string()은 ANSI로 바꾸며, 표현할 수 없는 글자가 있으면 예외를 던진다.
    // 증상: 프로젝트를 한글이 들어간 폴더("E2E Project 한글")에 두자 에디터 뷰포트가 매 프레임
    // "RuntimeError: No mapping for the Unicode character exists in the target multi-byte code page."로
    // 실패했고(RenderSystem의 경로 결합), 같은 폴더에서 빌드한 게임은 시작하자마자 같은 예외로 끝났다.
    // 경로가 ASCII뿐이면 두 해석이 같아서 테스트 환경에서는 드러나지 않았다.
    //
    // 규칙: 경로 문자열을 path로 만들 때는 PathFromUtf8, path를 문자열로 만들 때는 Utf8FromPath를 쓴다.
    // ifstream/stbi_load/tinyobj 등에 std::string 경로를 직접 넘기지 않는다(전부 ANSI로 연다).

    inline std::filesystem::path PathFromUtf8(std::string_view utf8)
    {
        return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
    }

    inline std::string Utf8FromPath(const std::filesystem::path& path)
    {
        const std::u8string s = path.u8string();
        return std::string(s.begin(), s.end());
    }

    // "/" 구분자로 된 UTF-8 문자열(로그·캐시 키·리플렉션 값에 쓰는 꼴).
    inline std::string GenericUtf8FromPath(const std::filesystem::path& path)
    {
        const std::u8string s = path.generic_u8string();
        return std::string(s.begin(), s.end());
    }
}
