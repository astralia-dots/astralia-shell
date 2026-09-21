#pragma once

#include <chrono>
#include <cstdint>

// bar style
enum class BarStyle { Islands,
                      Okinami };

constexpr int kBarStyleCount = 2;

inline constexpr const char *kBarStyleNames[kBarStyleCount] = {"islands", "okinami"};

inline constexpr const char *kBarStyleLabels[kBarStyleCount] = {"Islands", "Okinami"};

namespace bar_detail {
// bar height
constexpr int32_t kBarHeight = 40;

// pill layout & timing
constexpr float kPillPad = 10.0f;
constexpr float kCapsuleGap = 10.0f;
constexpr float kGroupSegmentGap = 20.0f;
constexpr float kWorkspaceOverviewGap = 8.0f;
constexpr float kPillExpandMs = 150.0f;
constexpr auto kPillCloseLingerMs = std::chrono::milliseconds(80);

// top margin
constexpr int32_t kBarTopMargin = 10;

// okinami layout
constexpr float kIslandPad = 6.0f;
constexpr float kIslandDividerHeightRatio = 0.4f;

// autohide
constexpr int32_t kAutoHideStripPx = 1;
constexpr float kAutoHideRevealMs = 150.0f;
constexpr float kAutoHideHideMs = 150.0f;
constexpr uint64_t kAutoHideAnimOwner = 1000;
} // namespace bar_detail
