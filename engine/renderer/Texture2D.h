#pragma once

#include <cstdint>
#include <memory>

namespace Engine
{
    /// <summary>
    /// GL 2D 텍스처 하나(RAII). 만든 GL 컨텍스트에서만 유효하다. 컨텍스트마다 따로 만들어야 하고,
    /// 캐시는 컨텍스트 단위로 소유하는 쪽(RenderSystem)이 맡는다(Mesh::loadFromFile 주석과 같은 이유).
    /// 렌더러가 asset/ 모듈에 의존하지 않도록 원시 픽셀만 받는다. asset -> renderer 연결은 RenderSystem이 한다.
    /// </summary>
    class Texture2D
    {
    public:
        // channels: 1(R), 2(RG), 3(RGB), 4(RGBA). 실패하면 nullptr(이유는 로그에 남긴다).
        // pixels는 이미지 파일 순서(맨 윗줄이 먼저, stb_image 기본)다. GL은 첫 줄을 v=0(아래)로 보므로
        // 여기서 뒤집어 올린다 - OBJ/절차 메시의 UV(v=0이 아래)와 맞추기 위해서다.
        static std::shared_ptr<Texture2D> Create(int width, int height, int channels, const uint8_t* pixels);

        ~Texture2D();
        Texture2D(const Texture2D&) = delete;
        Texture2D& operator=(const Texture2D&) = delete;

        uint32_t GetId() const { return id_; }
        int GetWidth() const { return width_; }
        int GetHeight() const { return height_; }

    private:
        Texture2D() = default;
        uint32_t id_ = 0;
        int width_ = 0;
        int height_ = 0;
    };
}
