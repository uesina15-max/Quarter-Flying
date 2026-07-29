#pragma once
#include <string>

namespace Engine
{
    class PathResolver
    {
    public:
        static void Init(const std::string& assetRoot, const std::string& shaderRoot)
        {
            s_assetRoot = assetRoot;
            s_shaderRoot = shaderRoot;
        }

        static std::string ResolveAsset(const std::string& path)
        {
            return s_assetRoot + path;
        }

        static std::string ResolveShader(const std::string& path)
        {
            // If the path already contains the shader root or is absolute, we might just return it.
            // For simplicity, just prefix if it's a raw filename. But DemoScene passes "assets/shaders/...".
            // Since DemoScene currently uses "assets/shaders/pbr.vert", we'll just return it directly 
            // if we use a relative path from the project root, or we fix the usages in DemoScene.
            // Let's assume path is relative to shaderRoot if it doesn't start with "assets"
            if (path.find("assets/") == 0) return path;
            return s_shaderRoot + path;
        }

    private:
        static inline std::string s_assetRoot = "./";
        static inline std::string s_shaderRoot = "./assets/shaders/";
    };
}
