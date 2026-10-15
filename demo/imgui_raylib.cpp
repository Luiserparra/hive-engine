#include "demo/imgui_raylib.h"

#include "imgui.h"
#include "raylib.h"
#include "rlgl.h"

#include <cstring>

namespace hive::demo {
namespace {

Texture2D g_font_texture{};
bool g_ok = false;

ImGuiKey map_key(int key) {
    switch (key) {
    case KEY_TAB:
        return ImGuiKey_Tab;
    case KEY_LEFT:
        return ImGuiKey_LeftArrow;
    case KEY_RIGHT:
        return ImGuiKey_RightArrow;
    case KEY_UP:
        return ImGuiKey_UpArrow;
    case KEY_DOWN:
        return ImGuiKey_DownArrow;
    case KEY_PAGE_UP:
        return ImGuiKey_PageUp;
    case KEY_PAGE_DOWN:
        return ImGuiKey_PageDown;
    case KEY_HOME:
        return ImGuiKey_Home;
    case KEY_END:
        return ImGuiKey_End;
    case KEY_INSERT:
        return ImGuiKey_Insert;
    case KEY_DELETE:
        return ImGuiKey_Delete;
    case KEY_BACKSPACE:
        return ImGuiKey_Backspace;
    case KEY_SPACE:
        return ImGuiKey_Space;
    case KEY_ENTER:
        return ImGuiKey_Enter;
    case KEY_ESCAPE:
        return ImGuiKey_Escape;
    case KEY_LEFT_CONTROL:
        return ImGuiKey_LeftCtrl;
    case KEY_RIGHT_CONTROL:
        return ImGuiKey_RightCtrl;
    case KEY_LEFT_SHIFT:
        return ImGuiKey_LeftShift;
    case KEY_RIGHT_SHIFT:
        return ImGuiKey_RightShift;
    case KEY_LEFT_ALT:
        return ImGuiKey_LeftAlt;
    case KEY_RIGHT_ALT:
        return ImGuiKey_RightAlt;
    default:
        if (key >= KEY_A && key <= KEY_Z) {
            return static_cast<ImGuiKey>(ImGuiKey_A + (key - KEY_A));
        }
        if (key >= KEY_ZERO && key <= KEY_NINE) {
            return static_cast<ImGuiKey>(ImGuiKey_0 + (key - KEY_ZERO));
        }
        return ImGuiKey_None;
    }
}

void update_keys() {
    ImGuiIO& io = ImGui::GetIO();
    constexpr int kKeys[] = {KEY_TAB,           KEY_LEFT,       KEY_RIGHT,       KEY_UP,
                             KEY_DOWN,          KEY_PAGE_UP,    KEY_PAGE_DOWN,   KEY_HOME,
                             KEY_END,           KEY_INSERT,     KEY_DELETE,      KEY_BACKSPACE,
                             KEY_SPACE,         KEY_ENTER,      KEY_ESCAPE,      KEY_LEFT_CONTROL,
                             KEY_RIGHT_CONTROL, KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_LEFT_ALT,
                             KEY_RIGHT_ALT};
    for (int key : kKeys) {
        const ImGuiKey mapped = map_key(key);
        if (mapped != ImGuiKey_None) {
            io.AddKeyEvent(mapped, IsKeyDown(key));
        }
    }
    for (int key = KEY_A; key <= KEY_Z; ++key) {
        io.AddKeyEvent(map_key(key), IsKeyDown(key));
    }
    for (int key = KEY_ZERO; key <= KEY_NINE; ++key) {
        io.AddKeyEvent(map_key(key), IsKeyDown(key));
    }
    io.AddKeyEvent(ImGuiMod_Ctrl, IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL));
    io.AddKeyEvent(ImGuiMod_Shift, IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT));
    io.AddKeyEvent(ImGuiMod_Alt, IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT));

    int codepoint = GetCharPressed();
    while (codepoint > 0) {
        io.AddInputCharacter(static_cast<unsigned int>(codepoint));
        codepoint = GetCharPressed();
    }
}

} // namespace

bool imgui_raylib_init() {
    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "imgui_raylib";
    io.BackendRendererName = "imgui_raylib";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    Image image = GenImageColor(width, height, BLANK);
    std::memcpy(image.data, pixels,
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
    g_font_texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTextureFilter(g_font_texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(g_font_texture, TEXTURE_WRAP_CLAMP);
    io.Fonts->SetTexID(static_cast<ImTextureID>(g_font_texture.id));
    g_ok = g_font_texture.id != 0;
    return g_ok;
}

void imgui_raylib_shutdown() {
    if (g_font_texture.id != 0) {
        UnloadTexture(g_font_texture);
        g_font_texture = {};
    }
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui::GetIO().Fonts->SetTexID(static_cast<ImTextureID>(0));
    }
    g_ok = false;
}

void imgui_raylib_new_frame() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize =
        ImVec2(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
    const float dt = GetFrameTime();
    io.DeltaTime = dt > 0.0f ? dt : (1.0f / 60.0f);

    const Vector2 mouse = GetMousePosition();
    io.AddMousePosEvent(mouse.x, mouse.y);
    io.AddMouseButtonEvent(0, IsMouseButtonDown(MOUSE_BUTTON_LEFT));
    io.AddMouseButtonEvent(1, IsMouseButtonDown(MOUSE_BUTTON_RIGHT));
    io.AddMouseButtonEvent(2, IsMouseButtonDown(MOUSE_BUTTON_MIDDLE));
    io.AddMouseWheelEvent(GetMouseWheelMoveV().x, GetMouseWheelMoveV().y);
    io.AddFocusEvent(IsWindowFocused());
    update_keys();

    const ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
    if (io.MouseDrawCursor) {
        HideCursor();
    } else {
        ShowCursor();
        switch (cursor) {
        case ImGuiMouseCursor_TextInput:
            SetMouseCursor(MOUSE_CURSOR_IBEAM);
            break;
        case ImGuiMouseCursor_ResizeNS:
            SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
            break;
        case ImGuiMouseCursor_ResizeEW:
            SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
            break;
        case ImGuiMouseCursor_Hand:
            SetMouseCursor(MOUSE_CURSOR_POINTING_HAND);
            break;
        default:
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
            break;
        }
    }
}

void imgui_raylib_draw() {
    const ImDrawData* draw = ImGui::GetDrawData();
    if (draw == nullptr || !g_ok) {
        return;
    }

    rlDrawRenderBatchActive();
    rlDisableBackfaceCulling();
    rlDisableDepthTest();
    rlEnableColorBlend();
    rlSetBlendMode(BLEND_ALPHA);
    rlEnableShader(rlGetShaderIdDefault());

    for (int n = 0; n < draw->CmdListsCount; ++n) {
        const ImDrawList* list = draw->CmdLists[n];
        const ImDrawVert* verts = list->VtxBuffer.Data;
        const ImDrawIdx* idx = list->IdxBuffer.Data;

        for (int cmd_i = 0; cmd_i < list->CmdBuffer.Size; ++cmd_i) {
            const ImDrawCmd& cmd = list->CmdBuffer[cmd_i];
            if (cmd.UserCallback != nullptr) {
                cmd.UserCallback(list, &cmd);
                continue;
            }

            const ImVec2 clip_min((cmd.ClipRect.x - draw->DisplayPos.x) * draw->FramebufferScale.x,
                                  (cmd.ClipRect.y - draw->DisplayPos.y) * draw->FramebufferScale.y);
            const ImVec2 clip_max((cmd.ClipRect.z - draw->DisplayPos.x) * draw->FramebufferScale.x,
                                  (cmd.ClipRect.w - draw->DisplayPos.y) * draw->FramebufferScale.y);
            if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                continue;
            }

            BeginScissorMode(static_cast<int>(clip_min.x), static_cast<int>(clip_min.y),
                             static_cast<int>(clip_max.x - clip_min.x),
                             static_cast<int>(clip_max.y - clip_min.y));

            unsigned int tex = static_cast<unsigned int>(cmd.GetTexID());
            if (tex == 0) {
                tex = g_font_texture.id;
            }

            // rlBegin(TRIANGLES) starts a new draw and resets texture to the 1x1 white.
            // Bind the atlas *after* begin or glyphs sample white and vanish.
            rlBegin(RL_TRIANGLES);
            rlSetTexture(tex);
            const unsigned int vtx_offset = cmd.VtxOffset;
            for (unsigned int i = 0; i < cmd.ElemCount; ++i) {
                const ImDrawVert& v = verts[idx[cmd.IdxOffset + i] + vtx_offset];
                const unsigned int color = v.col;
                rlColor4ub(static_cast<unsigned char>(color & 0xffu),
                           static_cast<unsigned char>((color >> 8) & 0xffu),
                           static_cast<unsigned char>((color >> 16) & 0xffu),
                           static_cast<unsigned char>((color >> 24) & 0xffu));
                rlTexCoord2f(v.uv.x, v.uv.y);
                rlVertex2f(v.pos.x, v.pos.y);
            }
            rlEnd();
            EndScissorMode();
        }
    }

    rlSetTexture(0);
    rlDrawRenderBatchActive();
    rlEnableBackfaceCulling();
}

} // namespace hive::demo
