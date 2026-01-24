// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include "private.h"

#include <config/functions.h>
#include <config/state.h>
#include <dialog/state.h>
#include <emuenv/state.h>
#include <gui/functions.h>
#include <interface.h>
#include <util/vector_utils.h>

#ifdef __ANDROID__
#include <SDL_system.h>
#include <jni.h>
#endif

namespace gui {

enum struct OverlayShowMask : int {
    Basic = 1, // Basic Vita Gamepad
    L2R2 = 2,
    TouchScreenSwitch = 4, // Button to switch between the front and back touchscreen
};

// idk where best to put this outside cpp
static bool overlay_hide = false;

int get_overlay_display_mask(const Config& cfg){
    int mask = 0;
    if (cfg.enable_gamepad_overlay) {
        mask = (int)OverlayShowMask::Basic;
        if(cfg.pstv_mode)
            mask |= (int)OverlayShowMask::L2R2;
        
    } else if (cfg.enable_gamepad_overlay && overlay_hide){
        mask |= (int)OverlayShowMask::TouchScreenSwitch;
    }

    // only show front back button
    if (cfg.overlay_show_touch_switch)
        mask |= (int)OverlayShowMask::TouchScreenSwitch;
    
    return mask;
}

#ifdef __ANDROID__

extern "C" {

JNIEXPORT void JNICALL
Java_org_vita3k_emulator_overlay_InputOverlay_setHideState(JNIEnv *env, jobject thiz, jboolean is_hide) {
    overlay_hide = is_hide;
}

}

void set_controller_overlay_state(int overlay_mask, bool edit, bool reset, bool portrait) {
    // retrieve the JNI environment.
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());

    // retrieve the Java instance of the SDLActivity
    jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());

    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    jclass clazz(env->GetObjectClass(activity));

    // find the identifier of the method to call
    jmethodID method_id = env->GetMethodID(clazz, "setControllerOverlayState", "(IZZZ)V");

    // effectively call the Java method
    env->CallVoidMethod(activity, method_id, overlay_mask, edit, reset, portrait);

    // clean up the local references.
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);
}

void set_controller_overlay_scale(float scale, float joystick) {
    // retrieve the JNI environment.
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());

    // retrieve the Java instance of the SDLActivity
    jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());

    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    jclass clazz(env->GetObjectClass(activity));

    // find the identifier of the method to call
    jmethodID method_id = env->GetMethodID(clazz, "setControllerOverlayScale", "(FF)V");

    // effectively call the Java method
    env->CallVoidMethod(activity, method_id, scale, joystick);

    // clean up the local references.
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);
}

void set_controller_overlay_opacity(int opacity) {
    // retrieve the JNI environment.
    JNIEnv *env = reinterpret_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());

    // retrieve the Java instance of the SDLActivity
    jobject activity = reinterpret_cast<jobject>(SDL_AndroidGetActivity());

    // find the Java class of the activity. It should be SDLActivity or a subclass of it.
    jclass clazz(env->GetObjectClass(activity));

    // find the identifier of the method to call
    jmethodID method_id = env->GetMethodID(clazz, "setControllerOverlayOpacity", "(I)V");

    // effectively call the Java method
    env->CallVoidMethod(activity, method_id, opacity);

    // clean up the local references.
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(clazz);
}

void draw_overlay_dialog(GuiState &gui, EmuEnvState &emuenv) {
    static bool overlay_editing = false;
    static const auto BUTTON_SIZE = ImVec2(120.f * emuenv.dpi_scale, 0.f);

    const ImVec2 display_size(emuenv.viewport_size.x, emuenv.viewport_size.y);
    const auto RES_SCALE = ImVec2(display_size.x / emuenv.res_width_dpi_scale, display_size.y / emuenv.res_height_dpi_scale);

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    if(emuenv.cfg.screenmode_pos == 3){
        center.y = center.y / 2;
    }
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("Overlay", &gui.controls_menu.controls_dialog, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowFontScale(RES_SCALE.x);
    ImGui::Spacing();

    const auto gmpd = ImGui::CalcTextSize("Gamepad Overlay").x;
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (gmpd / 2.f));
    ImGui::TextColored(GUI_COLOR_TEXT_MENUBAR, "Gamepad Overlay");
    ImGui::Spacing();
    if (ImGui::Checkbox("Show gamepad overlay ingame", &emuenv.cfg.enable_gamepad_overlay))
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);

    const char *overlay_edit_text = overlay_editing ? "Hide Gamepad Overlay" : "Modify Gamepad Overlay";
    ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (gmpd / 2.f));
    if (ImGui::Button(overlay_edit_text)) {
        overlay_editing = !overlay_editing;
        set_controller_overlay_state(overlay_editing ? get_overlay_display_mask(emuenv.cfg) : 0, overlay_editing);
    }
    ImGui::Spacing();
    if(overlay_editing){
        ImGui::Spacing();
        if (ImGui::SliderFloat("Overlay scale", &emuenv.cfg.overlay_scale, 0.25f, 4.0f, "%.3f", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_NoRoundToFormat | ImGuiSliderFlags_Logarithmic)) {
            set_controller_overlay_scale(emuenv.cfg.overlay_scale, emuenv.cfg.overlay_scale_joystick);
            config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
        }
        ImGui::Spacing();
        if (ImGui::SliderFloat("Overlay scale joystick", &emuenv.cfg.overlay_scale_joystick, 0.25f, 4.0f, "%.3f", ImGuiSliderFlags_NoInput | ImGuiSliderFlags_NoRoundToFormat | ImGuiSliderFlags_Logarithmic)) {
            set_controller_overlay_scale(emuenv.cfg.overlay_scale, emuenv.cfg.overlay_scale_joystick);
            config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
        }
        ImGui::Spacing();
        if (ImGui::SliderInt("Overlay opacity", &emuenv.cfg.overlay_opacity, 0, 100, "%d%%")) {
            set_controller_overlay_opacity(emuenv.cfg.overlay_opacity);
            config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
        }
        ImGui::Spacing();

        ImGui::SetCursorPosX((ImGui::GetWindowWidth() / 2.f) - (gmpd / 2.f));
        if (overlay_editing && ImGui::Button("Reset Gamepad")) {
           if(emuenv.cfg.screenmode_pos == 3){
               set_controller_overlay_state(get_overlay_display_mask(emuenv.cfg), true, true, true);   // portrait
           }else{
               set_controller_overlay_state(get_overlay_display_mask(emuenv.cfg), true, true, false);  // landscape
           }
           emuenv.cfg.overlay_scale = 1.0f;
           emuenv.cfg.overlay_scale_joystick = 1.0f;
           emuenv.cfg.overlay_opacity = 80;
           set_controller_overlay_scale(emuenv.cfg.overlay_scale, emuenv.cfg.overlay_scale_joystick);
           set_controller_overlay_opacity(emuenv.cfg.overlay_opacity);
           config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
       }
    }
    ImGui::Spacing();
    ImGui::Separator();

    if(emuenv.cfg.enable_gamepad_overlay){
        auto &emulator = gui.lang.settings_dialog.emulator;
        ImGui::Checkbox(emulator["sensor_enable"].c_str(), &emuenv.cfg.tiltsens);
        SetTooltipEx(emulator["sensors_description"].c_str());
        if (emuenv.cfg.tiltsens){
            ImGui::Checkbox(emulator["invert_gyro"].c_str(), &emuenv.cfg.invert_gyro);
            SetTooltipEx(emulator["invert_gyro_description"].c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        TextColoredCentered(GUI_COLOR_TEXT_TITLE, "Analog Stick Multiplier");
        ImGui::Spacing();
        auto &mult = emuenv.cfg.controller_analog_multiplier;
        if (ImGui::SliderFloat("##analog_multiplier", &mult, 0.1f, 2.f, "%.1fx", ImGuiSliderFlags_AlwaysClamp))
            config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
        SetTooltipEx("Analog multipiler can be used to change the sensitivity of your stick movements.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("L2/R2 triggers will be displayed only if PSTV mode is enabled.");
    }

    ImGui::Spacing();
    ImGui::Separator();

    if(ImGui::Checkbox("Show front/back touchscreen switch button.", &emuenv.cfg.overlay_show_touch_switch)){
        config::serialize_config(emuenv.cfg, emuenv.cfg.config_path);
    }
    
    auto &common = emuenv.common_dialog.lang.common;
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.f) - (BUTTON_SIZE.x / 2.f));
    if (ImGui::Button(common["close"].c_str(), BUTTON_SIZE)){
        overlay_editing = false;
        set_controller_overlay_state(0);
        gui.controls_menu.overlay_dialog = false;
    }

    ImGui::ScrollWhenDragging();
    ImGui::End();
}
#else

void set_controller_overlay_state(int overlay_mask, bool edit, bool reset, bool portrait) {}
void set_controller_overlay_scale(float scale, float joystick) {}
void set_controller_overlay_opacity(int opacity) {}

#endif

} // namespace gui
