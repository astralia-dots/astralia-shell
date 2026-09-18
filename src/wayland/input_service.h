#pragma once

#include "service/input_service.h"

namespace backend_wayland {

void keyboard_attach_seat(SeatCapabilityState &seat_state, void *seat);
void pointer_bind(PointerState &state, void *seat);
void pointer_release(PointerState &state);
void pointer_set_cursor_shape(PointerState &state, PointerShape shape);

} // namespace backend_wayland
