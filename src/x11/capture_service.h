#pragma once

#include "service/capture_service.h"

namespace backend_x11 {

void toplevel_export_request(ToplevelExportState &state, void *manager, void *shm, const std::string &address, int min_interval_ms);
void toplevel_export_prune(ToplevelExportState &state, const std::vector<std::string> &live_addresses);

} // namespace backend_x11
