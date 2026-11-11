#include "pch.h"

#include <cstring>
#include <vector>

// stb_image is a single-header public-domain PNG decoder, vendored
// like the other header-only third-party code (imgui, lua). It has
// no link dependency and no system package on openSUSE, so it is
// bundled, not system-linked. Only this translation unit includes
// it with the implementation defined.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../../vendors/stb/stb_image.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include "game/settings_png.h"
#include "game/vfs.h"

namespace SETTINGS_PNG {

    s_rgba_image Decode_Png(const uint8_t* data, size_t size) {
        s_rgba_image out;
        if (data == nullptr || size == 0) {
            return out;
        }
        int width = 0;
        int height = 0;
        int channels = 0;
        uint8_t* decoded = stbi_load_from_memory(data, (int)size,
            &width, &height, &channels, 4);
        if (decoded == nullptr || width <= 0 || height <= 0) {
            if (decoded != nullptr) {
                stbi_image_free(decoded);
            }
            return out;
        }
        const size_t bytes = (size_t)width * (size_t)height * 4;
        out.width = width;
        out.height = height;
        out.pixels.assign(decoded, decoded + bytes);
        stbi_image_free(decoded);
        return out;
    }

    s_rgba_image Load_Png(const char* vfs_path) {
        s_rgba_image out;
        if (vfs_path == nullptr || vfs_path[0] == '\0') {
            return out;
        }
        s_vfs_file* file = vfs::fopen(vfs_path, "rb");
        if (file == nullptr) {
            return out;
        }
        const int32_t size = vfs::fsize(file);
        if (size <= 0) {
            vfs::fclose(file);
            return out;
        }
        std::vector<uint8_t> bytes((size_t)size);
        if (vfs::fread(bytes.data(), (size_t)size, 1, file) != 1) {
            vfs::fclose(file);
            return out;
        }
        vfs::fclose(file);
        return Decode_Png(bytes.data(), bytes.size());
    }

} // namespace SETTINGS_PNG
