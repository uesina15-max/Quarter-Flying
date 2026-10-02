#include "Texture2D.h"
#include "../core/logging/Logger.h"
#include <GL/glew.h>
#include <cstring>
#include <vector>

namespace Engine
{
    std::shared_ptr<Texture2D> Texture2D::Create(int width, int height, int channels, const uint8_t* pixels)
    {
        if (width <= 0 || height <= 0 || !pixels || channels < 1 || channels > 4)
        {
            Logger::Log(LogLevel::Error, "Texture2D::Create - invalid input (w={}, h={}, channels={}, pixels={})",
                        width, height, channels, pixels ? "set" : "null");
            return nullptr;
        }

        const GLenum formats[] = { GL_RED, GL_RG, GL_RGB, GL_RGBA };
        const GLint internalFormats[] = { GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 };
        const GLenum format = formats[channels - 1];

        std::shared_ptr<Texture2D> texture(new Texture2D());
        texture->width_ = width;
        texture->height_ = height;

        glGenTextures(1, &texture->id_);
        if (texture->id_ == 0)
        {
            Logger::Log(LogLevel::Error, "Texture2D::Create - glGenTextures failed (no current GL context?)");
            return nullptr;
        }
        // 파일 순서(위->아래)를 GL 순서(아래->위)로 뒤집는다(헤더 주석 참고).
        const size_t rowBytes = static_cast<size_t>(width) * static_cast<size_t>(channels);
        std::vector<uint8_t> flipped(rowBytes * static_cast<size_t>(height));
        for (int y = 0; y < height; ++y)
        {
            std::memcpy(flipped.data() + static_cast<size_t>(height - 1 - y) * rowBytes,
                        pixels + static_cast<size_t>(y) * rowBytes, rowBytes);
        }

        glBindTexture(GL_TEXTURE_2D, texture->id_);
        // RGB 행의 바이트 수가 4의 배수가 아니면(예: 너비 3의 RGB) 기본 정렬(4)에서 행이 어긋난다.
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormats[channels - 1], width, height, 0, format, GL_UNSIGNED_BYTE, flipped.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        if (channels == 1)
        {
            // 흑백 텍스처가 빨간색으로 보이지 않게 R을 RGB 모두로 퍼뜨린다.
            const GLint swizzle[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
            glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
        }
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        return texture;
    }

    Texture2D::~Texture2D()
    {
        if (id_ != 0)
        {
            glDeleteTextures(1, &id_);
        }
    }
}
