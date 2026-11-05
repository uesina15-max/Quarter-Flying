#pragma once
#include <string>

namespace Engine {
struct CanvasComponent {
    bool enabled = true;
};
struct RectTransformComponent {
    float x = 0;
    float y = 0;
    float width = 240;
    float height = 64;
    int order = 0;
};
struct UITextComponent {
    std::string text = "Text";
    float fontSize = 24;
};
struct UIImageComponent {
    std::string imagePath = "";
};
struct UIButtonComponent {
    bool interactable = true;
};
}
