#pragma once

#include <string>

#include "config/launcher_config.h"

namespace launch_action_detail {

std::string shell_quote(const std::string &s);

}

namespace applications {
inline constexpr const char *browser = "browser";
inline constexpr const char *editor = "editor";
inline constexpr const char *file_manager = "file-manager";
inline constexpr const char *terminal = "terminal";
inline constexpr const char *open = "xdg-open";
} // namespace applications

std::string make_search_url(const std::string &text, const std::string &base);

std::string normalize_url(const std::string &text);

std::string resolve_web_target(const std::string &text, const std::string &base);

bool launch_non_drun(LauncherMode mode, const std::string &query);

void launch_submenu_action(const SubmenuEntry &entry, VisitStore &visits);

void launch_drun_app(const DesktopEntry &entry, VisitStore &visits);
