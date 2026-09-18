#pragma once

#include <X11/extensions/XInput2.h>

#include "service/input_service.h"

namespace backend_x11 {

void keyboard_attach_seat(SeatCapabilityState &seat_state, void *seat);
void pointer_bind(PointerState &state, void *seat);
void pointer_release(PointerState &state);
void pointer_set_cursor_shape(PointerState &state, PointerShape shape);

void handle_xi_device_event(SeatCapabilityState &seat_state, int xi_event_type, const XIDeviceEvent &event);

int xi_opcode();

} // namespace backend_x11
