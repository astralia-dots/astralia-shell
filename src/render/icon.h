#pragma once

#include <cstdint>
#include <string>

#include "render/text.h"

inline constexpr int ADASTRIA_SHELL_ICON_PX = 18;

RasterizedText rasterize_icon(const std::string &codepoint_utf8, int32_t scale = 1, int px = ADASTRIA_SHELL_ICON_PX);

Texture make_icon_texture(const std::string &codepoint_utf8, int32_t scale = 1);
