#include "pch.h"

#include "doctest/doctest.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "fakes/fake_test_support.h"
#include "game/allocate.h"
#include "game/animate.h"
#include "game/fonts.h"
#include "game/metal_button.h"
#include "game/sprite_art.h"
#include "game/ui_kit.h"
#include "game/video.h"
#include "game/vfs.h"

#include "miniz.h"

#ifndef SPRITE_ART_BASE_DATA_DIR
#define SPRITE_ART_BASE_DATA_DIR ""
#endif

namespace test_metal_button_detail {
    namespace fs = std::filesystem;

    // A well-formed native bitmap frame: 12-byte header, one frame
    // offset, width*height index bytes, zero padding to a multiple of 4.
    std::string Make_Frame(int16_t width, int16_t height, uint8_t fill) {
        const int32_t pixel_count = (int32_t)width * (int32_t)height;
        const int32_t size = (pixel_count + 16 + 3) / 4 * 4;
        std::string data((size_t)size, '\0');
        std::memcpy(&data[0], &width, sizeof(width));
        std::memcpy(&data[2], &height, sizeof(height));
        const int16_t frame_count = 1;
        std::memcpy(&data[6], &frame_count, sizeof(frame_count));
        data[11] = 1; // draw mode BITMAP
        const uint32_t offset = 16;
        std::memcpy(&data[12], &offset, sizeof(offset));
        for (int32_t i = 0; i < pixel_count; ++i) {
            data[16 + i] = (char)fill;
        }
        return data;
    }

    struct Scoped_Video_Page {
        std::vector<uint8_t> page;
        uint8_t* previous_page;

        Scoped_Video_Page()
            : page((size_t)video::g_screen_res_x * (size_t)video::g_screen_res_y, 0xFF),
              previous_page(video::current_video_page) {
            video::current_video_page = page.data();
        }

        ~Scoped_Video_Page() {
            video::current_video_page = previous_page;
        }

        [[nodiscard]] uint8_t At(int16_t x, int16_t y) const {
            return page[(size_t)y * (size_t)video::g_screen_res_x + (size_t)x];
        }
    };

    struct Print_Record {
        int16_t x;
        int16_t y;
        std::string text;
        bool active;
    };

    // A font with scripted widths: 10 px per character, 12 px tall,
    // so the button geometry tests measure the button, not the engine
    // font. Records prints so the test can tell which font drew the
    // label (label vs disabled).
    class Fake_Font : public ui_kit::Font {
      public:
        mutable std::vector<Print_Record> prints;

        int16_t Width(const char* text) const override {
            return (int16_t)(10 * std::strlen(text));
        }

        int16_t Height() const override {
            return 12;
        }

        void Print(int16_t x, int16_t y, const char* text,
            bool active) const override {
            prints.push_back({x, y, text, active});
        }
    };

    // Frames built from flat fills, so the test can tell the states
    // apart on the video page: normal 0x11, pressed 0x22, disabled
    // 0x33. Owns the backing frame bytes, so the Frame handles stay
    // valid for the test's lifetime.
    struct Test_Frames {
        std::string normal_data;
        std::string pressed_data;
        std::string disabled_data;
        metal_button::Frames frames;

        explicit Test_Frames(bool null_disabled = false)
            : normal_data(Make_Frame(85, 26, 0x11)),
              pressed_data(Make_Frame(85, 26, 0x22)),
              disabled_data(Make_Frame(85, 26, 0x33)) {
            frames.left_slice = 2;
            frames.right_slice = 2;
            frames.fallback_face = 242;
            frames.fallback_highlight = 247;
            frames.fallback_shadow = 232;
            frames.normal = ui_kit::Frame(
                (const s_animation_header*)normal_data.c_str());
            frames.pressed = ui_kit::Frame(
                (const s_animation_header*)pressed_data.c_str());
            if (!null_disabled) {
                frames.disabled = ui_kit::Frame(
                    (const s_animation_header*)disabled_data.c_str());
            }
        }
    };

    class Scoped_Test_Directory {
      public:
        Scoped_Test_Directory() {
            const auto unique_id = std::chrono::steady_clock::now().time_since_epoch().count();
            root_path_ = fs::temp_directory_path() / ("orion2re-metal-button-" + std::to_string(unique_id));
            fs::create_directories(root_path_);
        }

        ~Scoped_Test_Directory() {
            vfs::Shutdown();
            std::error_code error;
            fs::remove_all(root_path_, error);
        }

        [[nodiscard]] const fs::path& root_path() const {
            return root_path_;
        }

        void Write_Zip_File(const fs::path& relative_path, const std::vector<std::pair<std::string, std::string>>& entries) const {
            const fs::path full_path = root_path_ / relative_path;
            mz_zip_archive archive{};

            fs::create_directories(full_path.parent_path());
            mz_zip_zero_struct(&archive);
            DOCTEST_REQUIRE(mz_zip_writer_init_file(&archive, full_path.string().c_str(), 0) == MZ_TRUE);

            for (const auto& entry : entries) {
                DOCTEST_REQUIRE(mz_zip_writer_add_mem(&archive, entry.first.c_str(), entry.second.data(), entry.second.size(), MZ_BEST_COMPRESSION) == MZ_TRUE);
            }

            DOCTEST_REQUIRE(mz_zip_writer_finalize_archive(&archive) == MZ_TRUE);
            DOCTEST_REQUIRE(mz_zip_writer_end(&archive) == MZ_TRUE);
        }

      private:
        fs::path root_path_;
    };

    struct Scoped_Buffer_Pool {
        std::vector<uint8_t> storage;
        uint8_t* payload;

        explicit Scoped_Buffer_Pool(uint32_t payload_size)
            : storage(sizeof(allocate::s_alloc_header) + payload_size), payload(storage.data() + sizeof(allocate::s_alloc_header)) {
            auto* header = (allocate::s_alloc_header*)storage.data();
            header->current = 0;
            header->size = payload_size;
            header->reserved2 = 0;
        }
    };
} // namespace test_metal_button_detail

DOCTEST_TEST_SUITE("metal button") {
    DOCTEST_TEST_CASE("width is the label plus padding, height is the frame height") {
        using namespace test_metal_button_detail;
        Fake_Font label, disabled;
        Test_Frames test_frames;
        metal_button::Button button(test_frames.frames, label, disabled);
        // 10 px per character, 10 px padding per side.
        DOCTEST_CHECK(button.Width_For_Label("MORE") == 40 + 20);
        DOCTEST_CHECK(button.Width_For_Label("LESS") == 40 + 20);
        DOCTEST_CHECK(button.Width_For_Label("RETURN") == 60 + 20);
        DOCTEST_CHECK(button.Height() == 26);
    }

    DOCTEST_TEST_CASE("draw writes the state's frame and centres the label") {
        using namespace test_metal_button_detail;
        Fake_Font label, disabled;
        Test_Frames test_frames;
        metal_button::Button button(test_frames.frames, label, disabled);
        test::fakes::Reset_All_();
        Scoped_Video_Page page;
        button.Draw(100, 200, 60, "MORE", metal_button::State::kNormal);
        // The normal frame's fill lands on the page.
        DOCTEST_CHECK(page.At(100, 200) == 0x11);
        DOCTEST_CHECK(page.At(159, 225) == 0x11);
        // The label is centred: (60 - 40) / 2 = 10 in from the left,
        // (26 - 12) / 2 = 7 down from the top.
        DOCTEST_REQUIRE(label.prints.size() == 1);
        DOCTEST_CHECK(label.prints[0].x == 110);
        DOCTEST_CHECK(label.prints[0].y == 207);
        DOCTEST_CHECK(label.prints[0].text == "MORE");
        DOCTEST_CHECK(disabled.prints.empty());

        button.Draw(100, 200, 60, "MORE", metal_button::State::kPressed);
        DOCTEST_CHECK(page.At(100, 200) == 0x22);
        DOCTEST_REQUIRE(label.prints.size() == 2);
        DOCTEST_CHECK(label.prints[1].text == "MORE");

        button.Draw(100, 200, 60, "MORE", metal_button::State::kDisabled);
        DOCTEST_CHECK(page.At(100, 200) == 0x33);
        DOCTEST_REQUIRE(disabled.prints.size() == 1);
        DOCTEST_CHECK(disabled.prints[0].text == "MORE");
        DOCTEST_CHECK(disabled.prints[0].x == 110);
        DOCTEST_CHECK(disabled.prints[0].y == 207);
    }

    DOCTEST_TEST_CASE("draw falls back to the engine-drawn button when a frame is missing") {
        using namespace test_metal_button_detail;
        Fake_Font label, disabled;
        Test_Frames test_frames(/*null_disabled=*/true);
        test_frames.frames.normal = ui_kit::Frame();
        test_frames.frames.pressed = ui_kit::Frame();
        metal_button::Button button(test_frames.frames, label, disabled);
        test::fakes::Reset_All_();
        Scoped_Video_Page page;
        button.Draw(100, 200, 60, "MORE", metal_button::State::kNormal);
        // Flat face 242, highlight 247 top/left, shadow 232
        // bottom/right: one face fill plus four bevel fills.
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 5);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 242);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 247);
        DOCTEST_CHECK(test::fakes::g_fill_calls[2].color == 247);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[4].color == 232);
        DOCTEST_REQUIRE(label.prints.size() == 1);
        DOCTEST_CHECK(label.prints[0].text == "MORE");

        // Pressed inverts the bevel.
        test::fakes::Reset_All_();
        button.Draw(100, 200, 60, "MORE", metal_button::State::kPressed);
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 5);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 232);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 247);

        // Disabled is flat and low contrast.
        test::fakes::Reset_All_();
        button.Draw(100, 200, 60, "MORE", metal_button::State::kDisabled);
        DOCTEST_REQUIRE(test::fakes::g_fill_calls.size() == 5);
        DOCTEST_CHECK(test::fakes::g_fill_calls[0].color == 236);
        DOCTEST_CHECK(test::fakes::g_fill_calls[1].color == 235);
        DOCTEST_CHECK(test::fakes::g_fill_calls[3].color == 233);
        DOCTEST_REQUIRE(disabled.prints.size() == 1);
    }

    DOCTEST_TEST_CASE("load frames reads the three generated states from base.zip") {
        using namespace test_metal_button_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Zip_File("data/base.zip", {
            {"sprites/metal-button/normal.bin", Make_Frame(85, 26, 0x11)},
            {"sprites/metal-button/pressed.bin", Make_Frame(85, 26, 0x22)},
            {"sprites/metal-button/disabled.bin", Make_Frame(85, 26, 0x33)},
        });
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        Scoped_Buffer_Pool pool(65536);
        metal_button::Frames frames =
            metal_button::Load_Frames(pool.payload);
        DOCTEST_CHECK(!frames.normal.Is_Null());
        DOCTEST_CHECK(!frames.pressed.Is_Null());
        DOCTEST_CHECK(!frames.disabled.Is_Null());
        DOCTEST_CHECK(frames.left_slice == 2);
        DOCTEST_CHECK(frames.right_slice == 2);
    }

    DOCTEST_TEST_CASE("load frames fails soft when the frames are absent") {
        using namespace test_metal_button_detail;
        Scoped_Test_Directory test_dir;
        test_dir.Write_Zip_File("data/base.zip", {
            {"sprites/return/normal.bin", Make_Frame(85, 26, 0x11)},
        });
        DOCTEST_REQUIRE(vfs::Init(test_dir.root_path().string().c_str()) == 1);
        Scoped_Buffer_Pool pool(65536);
        metal_button::Frames frames =
            metal_button::Load_Frames(pool.payload);
        DOCTEST_CHECK(frames.normal.Is_Null());
        DOCTEST_CHECK(frames.pressed.Is_Null());
        DOCTEST_CHECK(frames.disabled.Is_Null());
    }
} // namespace
