#include "GameOverlay.h"
#include "Shader.h"
#include "../core/Utf8Path.h"
#include "../core/logging/Logger.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/GameUI.h"
#include "../ecs/Hierarchy.h"
#include "../ecs/ScriptComponent.h"
#include "../ecs/ScriptContext.h"
#ifndef QF_NO_PYTHON_SCRIPTING
#include <pybind11/pybind11.h>
#endif
#include <GL/glew.h>
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

namespace Engine {
namespace {
struct Widget {
    Entity entity;
    uint64_t uuid;
    float x, y, width, height;
    int order;
    std::string text, image;
    float fontSize = 24;
    bool button = false;
    bool operator==(const Widget&) const = default;
};

std::vector<Widget> Collect(ECSRegistry& registry, const std::string& root) {
    std::vector<Widget> result;
    const auto* rects = registry.GetComponentArray<RectTransformComponent>();
    if (!rects || !rects->Size()) return result;
    for (EntityID id : rects->GetEntityIDs()) {
        const Entity entity(id);
        const auto* rect = registry.GetComponent<RectTransformComponent>(entity);
        if (!rect || !(rect->width > 0 && rect->height > 0)) continue;
        Widget widget{entity, registry.GetUUID(entity).GetValue(), rect->x, rect->y,
                      rect->width, rect->height, rect->order};
        Entity ancestor = entity;
        bool visible = true;
        for (int depth = 0; ancestor.IsValid() && depth < kMaxHierarchyDepth; ++depth) {
            auto* canvas = registry.GetComponent<CanvasComponent>(ancestor);
            if (canvas && !canvas->enabled) { visible = false; break; }
            ancestor = GetParent(registry, ancestor);
            if (ancestor.IsValid()) {
                if (const auto* parent = registry.GetComponent<RectTransformComponent>(ancestor)) {
                    widget.x += parent->x;
                    widget.y += parent->y;
                }
            }
        }
        if (!visible) continue;
        if (const auto* label = registry.GetComponent<UITextComponent>(entity)) {
            widget.text = label->text;
            widget.fontSize = std::clamp(label->fontSize, 1.f, 512.f);
        }
        if (const auto* sprite = registry.GetComponent<UIImageComponent>(entity); sprite && !sprite->imagePath.empty())
            widget.image = Utf8FromPath(PathFromUtf8(root) / PathFromUtf8(sprite->imagePath));
        if (const auto* button = registry.GetComponent<UIButtonComponent>(entity)) widget.button = button->interactable;
        if (widget.button || !widget.text.empty() || !widget.image.empty()) result.push_back(std::move(widget));
    }
    std::sort(result.begin(), result.end(), [](const Widget& left, const Widget& right) {
        return std::tie(left.order, left.uuid) < std::tie(right.order, right.uuid);
    });
    return result;
}

void Notify(ECSRegistry& registry, Entity target, const char* method) {
#ifndef QF_NO_PYTHON_SCRIPTING
    if (!target.IsValid()) return;
    auto* script = registry.GetComponent<ScriptComponent>(target);
    if (!script || !script->pyInstance) return;
    ScriptContext::ScopedRegistry context(&registry);
    try {
        auto object = *static_cast<pybind11::object*>(script->pyInstance);
        if (pybind11::hasattr(object, method)) object.attr(method)();
    } catch (const std::exception& error) {
        Logger::Error("Game UI {}: {}", method, error.what());
    }
#endif
}

void Paint(Gdiplus::Graphics& graphics, const Widget& widget, bool hovered) {
    using namespace Gdiplus;
    RectF box(widget.x, widget.y, widget.width, widget.height);
    if (widget.button) {
        SolidBrush background(hovered ? Color(255, 47, 110, 156) : Color(255, 39, 55, 72));
        graphics.FillRectangle(&background, box);
    }
    if (!widget.image.empty()) {
        Bitmap sprite(PathFromUtf8(widget.image).c_str());
        if (sprite.GetLastStatus() == Ok) graphics.DrawImage(&sprite, box);
        else Logger::Warning("Game UI image could not be read: {}", widget.image);
        if (widget.button && hovered) {
            SolidBrush highlight(Color(40, 255, 255, 255));
            graphics.FillRectangle(&highlight, box);
        }
    }
    if (!widget.text.empty()) {
        Font font(L"Segoe UI", widget.fontSize, FontStyleRegular, UnitPixel);
        SolidBrush foreground(Color(255, 255, 255, 255));
        StringFormat centered;
        centered.SetAlignment(StringAlignmentCenter);
        centered.SetLineAlignment(StringAlignmentCenter);
        const auto label = PathFromUtf8(widget.text).wstring();
        graphics.DrawString(label.c_str(), static_cast<INT>(label.size()), &font, box, &centered, &foreground);
    }
}
}

struct GameOverlay::State {
    ULONG_PTR token = 0;
    Shader shader;
    GLuint vao = 0, texture = 0;
    int width = 0, height = 0;
    std::vector<Widget> previous;
    std::vector<InputEvent> events;
    Entity hover, pressed, paintedHover;
    bool hasPointer = false;
    int mouseX = 0, mouseY = 0;

    bool Initialize() {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&token, &input, nullptr) != Gdiplus::Ok) return false;
        if (!shader.loadFromSource(R"(#version 330 core
out vec2 uv;
void main() {
    vec2 point = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = vec2(point.x, 1.0 - point.y);
    gl_Position = vec4(point * 2.0 - 1.0, 0, 1);
})", R"(#version 330 core
in vec2 uv;
uniform sampler2D canvas;
out vec4 color;
void main() { color = texture(canvas, uv); }
)")) return false;
        glGenVertexArrays(1, &vao);
        glGenTextures(1, &texture);
        return vao && texture;
    }
    ~State() {
        if (texture) glDeleteTextures(1, &texture);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (token) Gdiplus::GdiplusShutdown(token);
    }
    Entity Hit(const std::vector<Widget>& widgets) const {
        if (hasPointer) for (auto it = widgets.rbegin(); it != widgets.rend(); ++it)
            if (it->button && mouseX >= it->x && mouseX < it->x + it->width &&
                mouseY >= it->y && mouseY < it->y + it->height) return it->entity;
        return {};
    }
    void UpdateHover(ECSRegistry& registry, Entity target) {
        if (target == hover) return;
        Notify(registry, hover, "on_ui_exit");
        hover = target;
        Notify(registry, hover, "on_ui_enter");
    }
    void HandleInput(ECSRegistry& registry, const std::vector<Widget>& widgets) {
        for (const auto& event : events) {
            hasPointer = event.type != InputEventType::FocusLost;
            mouseX = event.mouseX;
            mouseY = event.mouseY;
            UpdateHover(registry, Hit(widgets));
            if (!hasPointer) pressed = {};
            else if (event.type == InputEventType::MouseButtonDown) pressed = hover;
            else if (event.type == InputEventType::MouseButtonUp) {
                if (pressed == hover) Notify(registry, hover, "on_ui_click");
                pressed = {};
            }
        }
        events.clear();
        UpdateHover(registry, Hit(widgets));
    }
    bool Upload(const std::vector<Widget>& widgets, int newWidth, int newHeight) {
        using namespace Gdiplus;
        std::vector<BYTE> pixels(static_cast<size_t>(newWidth) * newHeight * 4);
        Bitmap bitmap(newWidth, newHeight, newWidth * 4, PixelFormat32bppPARGB, pixels.data());
        if (bitmap.GetLastStatus() != Ok) return false;
        {
            Graphics graphics(&bitmap);
            graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
            for (const auto& widget : widgets) Paint(graphics, widget, widget.entity == hover);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, newWidth, newHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        previous = widgets;
        paintedHover = hover;
        width = newWidth;
        height = newHeight;
        return true;
    }
};

GameOverlay::GameOverlay() : state(std::make_unique<State>()) {}
GameOverlay::~GameOverlay() = default;
void GameOverlay::QueuePointer(const InputEvent& event) {
    if (event.type == InputEventType::FocusLost || event.type == InputEventType::MouseMove ||
        ((event.type == InputEventType::MouseButtonDown || event.type == InputEventType::MouseButtonUp) &&
         event.mouseButton == MouseButton::Left)) state->events.push_back(event);
}

void GameOverlay::Frame(ECSRegistry& registry, const std::string& assetRoot, bool interactive) {
    auto widgets = Collect(registry, assetRoot);
    if (interactive) {
        state->HandleInput(registry, widgets);
    } else {
        state->events.clear();
        state->pressed = {};   // 멈춘 동안 뗀 버튼을 재개 후 클릭으로 오해하지 않게
    }
    if (widgets.empty()) return;
    if (!state->token && !state->Initialize()) return;
    if (!state->texture) return;
    GLint viewport[4], oldProgram, oldVao, oldTexture, oldUnit, srcRGB, dstRGB, srcAlpha, dstAlpha;
    glGetIntegerv(GL_VIEWPORT, viewport);
    int width = viewport[2], height = viewport[3];
    if (width <= 0 || height <= 0 || static_cast<int64_t>(width)*height > 33554432) return;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &oldVao);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldUnit);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glBindTexture(GL_TEXTURE_2D, state->texture);
    if (state->width != width || state->height != height || widgets != state->previous || state->hover != state->paintedHover)
        state->Upload(widgets, width, height);
    glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
    glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
    bool blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    state->shader.use();
    state->shader.setInt("canvas", 0);
    glBindVertexArray(state->vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(oldVao);
    glUseProgram(oldProgram);
    glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
    if (!blend) glDisable(GL_BLEND);
    if (depth) glEnable(GL_DEPTH_TEST);
    glBindTexture(GL_TEXTURE_2D, oldTexture);
    glActiveTexture(oldUnit);
}
}
