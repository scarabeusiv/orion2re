#include "pch.h"

#include "fakes/fake_test_support.h"

// Minimal recording fakes for the fonts::/graphics:: surface ui_kit
// uses: Set_Font_Style, Set_Font_Spacing, Get_String_Width,
// Get_Font_Height, Print, Fill, Box. Bodies copy the
// fields-runtime fake
// (fake_fields_runtime.cpp), which this target cannot link: that
// fake duplicates symbols the production set already defines
// (compat.cpp's Query_Memory_Status / Show_Message_Box) and the
// test-state globals. The call vectors live in fake_test_state.cpp,
// like the other recording fakes.
namespace fonts {
    void __cdecl Set_Font_Style(int32_t font_index, const s_colors* new_colors) {
        test::fakes::g_font_style = static_cast<int16_t>(font_index);
        if (new_colors != nullptr) {
            test::fakes::g_font_color = *new_colors;
        }
    }

    void __cdecl Set_Font_Spacing(int16_t spacing) {
        test::fakes::g_font_spacing = spacing;
    }

    int16_t __cdecl Get_String_Width(const char* str) {
        (void)str;
        return test::fakes::g_string_width;
    }

    int16_t __cdecl Get_Font_Height() {
        return test::fakes::g_font_height;
    }

    void __cdecl Print(int16_t x, int16_t y, const char* str) {
        test::fakes::g_print_calls.push_back({x, y, str, str != nullptr ? std::string(str) : std::string()});
    }
} // namespace fonts

namespace graphics {
    void __cdecl Fill(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t color) {
        test::fakes::g_fill_calls.push_back({x1, y1, x2, y2, color});
    }

    void __cdecl Box(int16_t x, int16_t y, int16_t width, int16_t height, int16_t thickness, uint8_t color) {
        test::fakes::g_box_calls.push_back({x, y, width, height, thickness, color});
    }
} // namespace graphics
