#pragma once

#include <cstdint>

// layer namespace
constexpr const char *kPolkitNamespace = "astralia-shell-polkit";

// card layout & animation
constexpr float kPolkitAnimMs = 350.0f;
constexpr float kPolkitScaleHidden = 0.0f;
constexpr float kPolkitBorderWidth = 5.0f;
constexpr float kPolkitCardPad = 30.0f;
constexpr float kPolkitCardRadius = 20.0f;
constexpr float kPolkitCardWidth = 480.0f;
constexpr float kPolkitDotMargin = 8.0f;
constexpr float kPolkitDotSize = 16.0f;
constexpr float kPolkitFieldHeight = 55.0f;
constexpr float kPolkitFieldRadius = 27.0f;
constexpr float kPolkitSpacing = 16.0f;

// text line heights
constexpr float kPolkitTitleLineH = 20.0f;
constexpr float kPolkitMessageLineH = 16.0f;
constexpr float kPolkitSupplementaryLineH = 14.0f;

// animation owners
constexpr uint64_t kPolkitCardScaleOwner = 10;
constexpr uint64_t kPolkitDotAnimBase = 1000;
