#pragma once

#include <X11/Xlib.h>

namespace x11_atoms {

Atom net_wm_strut();
Atom net_wm_strut_partial();
Atom net_wm_desktop();
Atom net_wm_window_type();
Atom net_wm_window_type_dock();
Atom net_wm_state();
Atom net_wm_state_above();
Atom net_wm_state_below();
Atom net_wm_name();
Atom utf8_string();
Atom wm_protocols();
Atom wm_delete_window();

} // namespace x11_atoms
