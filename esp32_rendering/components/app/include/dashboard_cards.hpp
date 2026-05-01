#pragma once

#include "dashboard_snapshot.hpp"
#include <1bit/core/framebuffer.hpp>
#include <1bit/render/bitmap_font.hpp>
#include <1bit/render/pattern.hpp>
#include <cstdint>

namespace app {

enum class CardLayout : uint8_t {
    Headline,
    Trends,
};

enum class MetricRef : uint8_t {
    Cpu, Memory, Disk, Uptime, Temp, Screens,
    Custom,  // headline = first chars of raw output
    None,    // for layouts that don't read a single metric (Trends)
};

struct DashboardCard {
    CardLayout       layout         = CardLayout::Headline;
    MetricRef        source         = MetricRef::None;
    int8_t           command_index  = -1;  // index into snapshot.command_outputs[]
                                            // REQUIRED (>=0) for Headline,
                                            // -1 for Trends
    char             label[24]      = {};
    onebit::Pattern  signature      {};
};

/// Render one card to the full 400x300 framebuffer. Caller must clear fb
/// before calling (DashboardScreen does this once per frame).
/// Vector-font is a free-function API, so no font instance.
void renderCard(onebit::IFramebuffer& fb, const onebit::BitmapFont& font,
                const DashboardCard& card, const DashboardSnapshot& snapshot);

/// Render the page-indicator pip strip at y=282..295 (h=14).
/// `underline_x` is the eased pixel x produced by Animator::value(); render
/// does no time math.
void renderPipStrip(onebit::IFramebuffer& fb, int current_card, int prev_card,
                    int card_count, int16_t underline_x);

/// Full-screen "No dashboard commands" message, centered.
void renderEmptyState(onebit::IFramebuffer& fb, const onebit::BitmapFont& font);

} // namespace app
