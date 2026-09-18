#pragma once

struct WaylandState;

void backend_poll_flush(WaylandState &app);
int backend_poll_fd(WaylandState &app);
void backend_poll_dispatch(WaylandState &app);
