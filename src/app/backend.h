#pragma once

enum class Backend { Wayland,
                     X11 };

struct BackendConnection {
    Backend backend;
    void *display = nullptr;
};

BackendConnection backend_connect();

Backend active_backend();

void *active_display();

void backend_wait_dispatch();
