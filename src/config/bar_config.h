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

// workspace widget
constexpr float kWorkspacePillHeight = 12.0f;
constexpr float kWorkspacePillSpacing = 5.0f;
constexpr float kWorkspaceActiveWidthScale = 2.0f;
constexpr float kWorkspacePillAnimMs = 100.0f;
constexpr uint64_t kWorkspacePillOwnerBase = 200;

// dock widget
constexpr uint64_t kDockWidgetAnimOwnerBase = 300;
} // namespace bar_detail

// control center panel
constexpr float kControlCenterPanelWidth = 360.0f;
constexpr float kControlCenterCardGap = 10.0f;

// control center profile
constexpr float kProfileAvatarSize = 100.0f;
constexpr float kProfileBorderWidth = 2.0f;
constexpr float kProfileAvatarRingWidth = 3.0f;
constexpr float kProfileRadius = 12.0f;
constexpr float kProfileVerticalPadding = 24.0f;
constexpr float kProfileTopPadding = 12.0f;
constexpr float kProfileAvatarGap = 8.0f;
constexpr float kProfileInfoSpacing = 2.0f;
constexpr float kProfileSettingsHitPadding = 8.0f;

// control center battery
constexpr float kBatteryBarHeight = 6.0f;
constexpr float kBatteryBarRadius = 3.0f;
constexpr float kBatteryHeaderSpacing = 10.0f;
constexpr float kBatteryRowSpacing = 8.0f;

// control center media
constexpr float kMediaThumbSize = 72.0f;
constexpr float kMediaThumbRadius = 8.0f;
constexpr float kMediaTitleLeftMargin = 12.0f;
constexpr float kMediaTitleSpacing = 3.0f;
constexpr float kMediaProgressRowHeight = 20.0f;
constexpr float kMediaProgressTopMargin = 10.0f;
constexpr float kMediaCtrlRowHeight = 32.0f;
constexpr float kMediaCtrlTopMargin = 8.0f;
constexpr float kMediaCtrlSpacing = 8.0f;
constexpr float kMediaSideBtnSize = 28.0f;
constexpr float kMediaSideBtnRadius = 14.0f;
constexpr float kMediaPlayBtnSize = 32.0f;
constexpr float kMediaPlayBtnRadius = 16.0f;

// control center volume
constexpr float kVolumeCardSpacing = 8.0f;
constexpr float kVolumeRowSpacing = 4.0f;
constexpr float kVolumeLabelRowSpacing = 4.0f;
constexpr float kVolumeDeviceTextMaxWidth = 148.0f;
constexpr float kVolumeSliderRowHeight = 24.0f;
constexpr float kVolumeCardSliderTrackHeight = 6.0f;
constexpr float kVolumeSliderPctGap = 8.0f;
constexpr float kVolumePctTextWidth = 40.0f;
constexpr float kVolumePctMuteGap = 6.0f;
constexpr float kVolumeMuteBtnSize = 22.0f;
constexpr float kVolumeMuteBtnRadius = 11.0f;

// control center brightness
constexpr float kBrightnessSliderRowHeight = 24.0f;
constexpr float kBrightnessSliderTrackHeight = 6.0f;
constexpr float kBrightnessSliderPctGap = 8.0f;
constexpr float kBrightnessPctTextWidth = 40.0f;
constexpr float kBrightnessIconGap = 10.0f;
constexpr float kBrightnessKeyStep = 0.01f;

// resource panel
constexpr float kResourcePanelWidth = 400.0f;
constexpr float kResourcePanelMaxHeight = 680.0f;
constexpr float kResourceCardGap = 10.0f;

// resource gauge cards
constexpr float kGaugeDiameter = 108.0f;
constexpr float kGaugeStroke = 8.0f;
constexpr float kGaugeRowGap = 16.0f;
constexpr float kGaugeCenterLineGap = 2.0f;
constexpr float kUsageWarnThreshold = 0.7f;
constexpr float kUsageCriticalThreshold = 0.9f;
constexpr float kTempWarnCelsius = 70.0f;
constexpr float kTempCriticalCelsius = 85.0f;
constexpr float kTempGaugeMaxCelsius = 100.0f;

// resource memory card
constexpr float kMemoryBarHeight = 6.0f;
constexpr float kMemoryBarRadius = 3.0f;
constexpr float kMemoryBarMinFill = 6.0f;
constexpr float kMemoryLabelBarGap = 6.0f;
constexpr float kMemoryLineGap = 12.0f;

// resource gauge colors
constexpr const char *kGaugeColorCpuHex = "#ef4444";
constexpr const char *kGaugeColorGpuHex = "#a855f7";
constexpr const char *kGaugeColorRamHex = "#3b82f6";
constexpr const char *kGaugeColorDiskHex = "#22c55e";
constexpr const char *kTempWarnColorHex = "#f97316";

// clock panel
constexpr float kClockPanelWidth = 504.0f;
constexpr float kClockWeekdayRowHeight = 22.0f;
constexpr float kClockGridTopGap = 2.0f;
constexpr float kClockCellCirclePadding = 4.0f;
constexpr float kClockNavButtonSize = 20.0f;
constexpr float kClockNavButtonGap = 6.0f;
constexpr float kClockColumnGap = 16.0f;
constexpr float kClockGridHeaderHeight = 24.0f;
constexpr float kClockGridHeaderGap = 15.0f;
constexpr float kClockWeekdayLineHeight = 26.0f;
constexpr float kClockDateLineHeight = 18.0f;
constexpr float kClockLeftLineGap = 2.0f;
constexpr int kClockBigDayFontPx = 64;
constexpr float kClockBigDayRowHeight = 74.0f;
constexpr float kClockBigDayGap = 6.0f;
constexpr float kClockWeekLineHeight = 16.0f;

// volume panel
constexpr float kVolumeLabelRowHeight = 20.0f;
constexpr float kVolumeRowHeight = 24.0f;
constexpr float kVolumeSliderHeight = 20.0f;
constexpr float kVolumeSliderTrackHeight = 6.0f;
constexpr float kVolumeAppSliderHeight = 16.0f;
constexpr float kVolumeAppSliderTrackHeight = 5.0f;
constexpr float kVolumePercentLabelWidth = 40.0f;
constexpr float kVolumeSliderRightGap = 8.0f;
constexpr float kVolumeLabelWidthCap = 180.0f;
constexpr float kVolumeSectionHeaderPad = 6.0f;
constexpr float kVolumeAppListTopGap = 4.0f;
constexpr float kVolumeAppRowBottomPad = 8.0f;
constexpr float kVolumeDeviceRowBottomPad = 6.0f;
constexpr float kVolumeDeviceIndicatorSize = 14.0f;
constexpr float kVolumeDeviceIndicatorRadius = 7.0f;
constexpr float kVolumeDeviceIndicatorDotSize = 6.0f;
constexpr float kVolumeDeviceIndicatorDotRadius = 3.0f;
constexpr std::chrono::milliseconds kVolumePeekMs{2000};
constexpr std::chrono::milliseconds kVolumePeekReadyDelayMs{1000};
constexpr float kVolumeTextRowHeight = 18.0f;
constexpr float kVolumeAppRowHeight =
    kVolumeTextRowHeight + kVolumeAppListTopGap + kVolumeAppSliderHeight +
    kVolumeAppRowBottomPad;
constexpr float kVolumeDeviceRowHeight =
    kVolumeTextRowHeight + kVolumeDeviceRowBottomPad;
constexpr float kVolumeSectionHeaderHeight =
    kVolumeTextRowHeight + kVolumeSectionHeaderPad;
constexpr float kVolumeDividerRowHeight = 1.0f;

// tray menu
constexpr float kTrayMenuWidth = 220.0f;
constexpr float kTrayMenuPadding = 4.0f;
constexpr float kTrayMenuItemHeight = 28.0f;
constexpr float kTrayMenuRowPaddingH = 8.0f;
constexpr float kTrayMenuLabelWidthOffset = 24.0f;
constexpr float kTrayMenuSeparatorHeight = 8.0f;
constexpr float kTrayMenuSeparatorLineHeight = 1.0f;
constexpr float kTrayMenuSeparatorWidthOffset = 12.0f;
constexpr float kTrayMenuBorderWidth = 1.0f;
constexpr float kTrayMenuRadius = 8.0f;

// tray panel
constexpr float kTrayCellSize = 40.0f;
constexpr float kTrayIconTargetSize = 20.0f;
constexpr int kTrayColumns = 4;
constexpr float kTrayGridGap = 4.0f;

// network panel
constexpr float kNetErrorBannerHeight = 48.0f;
constexpr float kNetworkEthernetBannerHeight = 40.0f;
constexpr float kNetworkUnavailableStateHeight = 72.0f;
constexpr float kNetworkScanningStateHeight = 48.0f;
constexpr float kNetworkSectionGapSmall = 20.0f;
constexpr float kNetworkSectionGapLarge = 24.0f;
constexpr float kNetworkConnectDisabledAlpha = 0.4f;
constexpr uint64_t kNetworkPasswordTypeAnimOwnerBase = 10000;
constexpr uint64_t kNetworkPasswordRowSlideOwner = 10512;
constexpr float kNetworkCaptiveBg[4] = {1.0f, 0.76f, 0.03f, 0.15f};
constexpr float kNetworkCaptiveFg[4] = {1.0f, 0.7569f, 0.0275f, 1.0f};

// battery panel
constexpr float kBatteryEmptyStateHeight = 72.0f;
constexpr float kBatteryTextRowHeight = 18.0f;
constexpr float kBatteryPanelBarHeight = 6.0f;
constexpr float kBatteryPanelBarRadius = 3.0f;
constexpr float kBatteryBarTopGap = 12.0f;
constexpr float kBatteryDeviceRowBottomPad = 14.0f;
constexpr float kBatteryDeviceRowHeight =
    kBatteryTextRowHeight + kBatteryBarTopGap + kBatteryPanelBarHeight +
    kBatteryDeviceRowBottomPad;

// bluetooth panel
constexpr float kBtEmptyStateHeight = 72.0f;
constexpr float kBtSectionGapSmall = 20.0f;
constexpr float kBtSectionGapLarge = 24.0f;
