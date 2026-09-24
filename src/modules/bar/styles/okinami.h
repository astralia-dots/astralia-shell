#pragma once

#include <cstdint>
#include <vector>

#include "modules/bar/styles/geometry.h"

const BarStyleSpec &okinami_style_spec();

std::vector<uint8_t> fillet_rgba(int size, bool circle_on_right);
