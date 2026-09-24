#include <cassert>
#include <cstdio>
#include <filesystem>

#include "render/image.h"

static bool has_nonzero_byte(const unsigned char *data, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (data[i] != 0)
            return true;
    return false;
}

static void test_decode_png() {
    int width = 0, height = 0;
    unsigned char *data =
        load_image_decode(ASTRALIA_SHELL_DEFAULT_WALLPAPER, width, height);
    assert(data);
    assert(width == 1920);
    assert(height == 1080);
    assert(has_nonzero_byte(data, static_cast<size_t>(width) * height * 4));
    delete[] data;
}

static void test_decode_svg() {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "astralia-shell-test-icon.svg";
    std::FILE *f = std::fopen(path.c_str(), "w");
    assert(f);
    std::fputs("<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
               "<rect width='24' height='24' fill='red'/></svg>",
               f);
    std::fclose(f);

    int width = 0, height = 0;
    unsigned char *data = load_image_decode(path.string(), width, height, 32);
    assert(data);
    assert(width == 32);
    assert(height == 32);
    assert(has_nonzero_byte(data, static_cast<size_t>(width) * height * 4));
    assert(data[0] == 255 && data[1] == 0 && data[2] == 0 && data[3] == 255);
    delete[] data;

    assert(!load_image_decode(path.string(), width, height));

    std::filesystem::remove(path);
}

static void test_decode_truncated() {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "astralia-shell-test-truncated";
    std::filesystem::copy_file(ASTRALIA_SHELL_DEFAULT_WALLPAPER, path, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::resize_file(path, std::filesystem::file_size(path) / 2);
    int width = 0, height = 0;
    assert(!load_image_decode(path.string(), width, height));

    std::FILE *f = std::fopen(path.c_str(), "wb");
    assert(f);
    std::fputs("\xFF\xD8\xFF\xE0garbage", f);
    std::fclose(f);
    assert(!load_image_decode(path.string(), width, height));

    std::filesystem::remove(path);
}

void test_image_decode() {
    test_decode_png();
    test_decode_svg();
    test_decode_truncated();
}
