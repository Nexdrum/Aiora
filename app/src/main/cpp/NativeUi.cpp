#include "NativeUi.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <array>

namespace aiora {
namespace {

constexpr NativeUi::Rgb kBg{0.0627f, 0.0706f, 0.0863f};
constexpr NativeUi::Rgb kTop{0.0784f, 0.0902f, 0.1137f};
constexpr NativeUi::Rgb kPanel{0.0863f, 0.1020f, 0.1294f};
constexpr NativeUi::Rgb kButton{0.1373f, 0.1569f, 0.2000f};
constexpr NativeUi::Rgb kCyan{0.0f, 0.80f, 0.80f};

constexpr std::array<NativeUi::Rgb, 12> kPitchColors{{
    {0.2275f, 1.0000f, 0.0000f}, // C
    {0.0000f, 1.0000f, 0.9255f},
    {0.0000f, 0.5608f, 1.0000f}, // D
    {0.0588f, 0.0000f, 0.9843f},
    {0.3882f, 0.0000f, 0.7451f},
    {0.4314f, 0.0000f, 0.5020f},
    {0.5961f, 0.0000f, 0.0000f},
    {0.7843f, 0.0000f, 0.0000f},
    {0.9529f, 0.0000f, 0.0000f},
    {1.0000f, 0.4706f, 0.0000f},
    {1.0000f, 0.9373f, 0.0000f},
    {0.6667f, 1.0000f, 0.0000f},
}};

NativeUi::Rgb mix(NativeUi::Rgb a, NativeUi::Rgb b, float amount) noexcept {
    amount = std::clamp(amount, 0.0f, 1.0f);
    return {
        a.r + (b.r - a.r) * amount,
        a.g + (b.g - a.g) * amount,
        a.b + (b.b - a.b) * amount
    };
}

} // namespace

void NativeUi::resize(int width, int height) noexcept {
    width_ = std::max(0, width);
    height_ = std::max(0, height);
}

void NativeUi::setPitchActive(int midi, bool active) noexcept {
    if (midi < kGridLow || midi > kGridHigh) return;
    active_[static_cast<size_t>(midi - kGridLow)] = active;
}

void NativeUi::clearPitchActivity() noexcept {
    active_.fill(false);
}

NativeUi::Rect NativeUi::navRect(int index) const noexcept {
    const float margin = std::max(4.0f, width_ * 0.008f);
    const float headerH = std::max(38.0f, height_ * 0.105f);
    const float navH = std::max(38.0f, height_ * 0.105f);
    const float gap = std::max(3.0f, width_ * 0.004f);
    const float available = std::max(
        0.0f, static_cast<float>(width_) - margin * 2.0f - gap * 5.0f);
    const float buttonW = available / 6.0f;

    return {
        margin + index * (buttonW + gap),
        headerH + gap,
        buttonW,
        navH - gap
    };
}

NativeUi::Rect NativeUi::contentRect() const noexcept {
    const float margin = std::max(4.0f, width_ * 0.008f);
    const auto lastNav = navRect(0);
    const float gap = std::max(4.0f, height_ * 0.010f);
    const float top = lastNav.y + lastNav.h + gap;
    return {
        margin,
        top,
        std::max(0.0f, static_cast<float>(width_) - margin * 2.0f),
        std::max(0.0f, static_cast<float>(height_) - top - margin)
    };
}

NativeUi::Rect NativeUi::gridRect(int visualRow, int column) const noexcept {
    const auto content = contentRect();
    const float gap = std::max(2.0f, std::min(width_, height_) * 0.006f);
    const float usableW = std::max(0.0f, content.w - gap * 6.0f);
    const float usableH = std::max(0.0f, content.h - gap * 6.0f);
    const float cell = std::max(1.0f, std::min(usableW / 7.0f, usableH / 7.0f));
    const float gridW = cell * 7.0f + gap * 6.0f;
    const float gridH = cell * 7.0f + gap * 6.0f;
    const float originX = content.x + (content.w - gridW) * 0.5f;
    const float originY = content.y + (content.h - gridH) * 0.5f;

    return {
        originX + column * (cell + gap),
        originY + visualRow * (cell + gap),
        cell,
        cell
    };
}

NativeUi::Rgb NativeUi::pitchColor(int midi) const noexcept {
    const int pc = ((midi % 12) + 12) % 12;
    return kPitchColors[static_cast<size_t>(pc)];
}

void NativeUi::fillRect(const Rect& rect, Rgb color) const noexcept {
    if (rect.w <= 0.0f || rect.h <= 0.0f || width_ <= 0 || height_ <= 0) return;

    const int x = std::max(0, static_cast<int>(rect.x));
    const int top = std::max(0, static_cast<int>(rect.y));
    const int w = std::max(0, std::min(width_ - x, static_cast<int>(rect.w)));
    const int h = std::max(0, std::min(height_ - top, static_cast<int>(rect.h)));
    if (w <= 0 || h <= 0) return;

    const int glY = height_ - top - h;
    glScissor(x, glY, w, h);
    glClearColor(color.r, color.g, color.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void NativeUi::drawGrid() const noexcept {
    for (int visualRow = 0; visualRow < 7; ++visualRow) {
        const int block = 6 - visualRow;
        for (int column = 0; column < 7; ++column) {
            const int midi = kGridLow + block * 7 + column;
            const bool active = active_[static_cast<size_t>(midi - kGridLow)];
            const auto color = pitchColor(midi);
            const auto rect = gridRect(visualRow, column);

            fillRect(rect, mix(kBg, color, active ? 0.95f : 0.62f));

            const float border = std::max(2.0f, rect.w * 0.055f);
            Rect inner{
                rect.x + border,
                rect.y + border,
                std::max(0.0f, rect.w - border * 2.0f),
                std::max(0.0f, rect.h - border * 2.0f)
            };
            fillRect(inner, mix(kPanel, color, active ? 0.50f : 0.16f));
        }
    }
}

void NativeUi::drawPlaceholder() const noexcept {
    const auto content = contentRect();
    fillRect(content, kPanel);

    const float gap = std::max(6.0f, content.w * 0.012f);
    const float rowH = std::max(28.0f, content.h * 0.12f);
    for (int i = 0; i < 4; ++i) {
        Rect row{
            content.x + gap,
            content.y + gap + i * (rowH + gap),
            std::max(0.0f, content.w - gap * 2.0f),
            rowH
        };
        fillRect(row, i == 0 ? mix(kButton, kCyan, 0.22f) : kButton);
    }
}

void NativeUi::render() const noexcept {
    if (width_ <= 0 || height_ <= 0) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    glScissor(0, 0, width_, height_);
    glClearColor(kBg.r, kBg.g, kBg.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    const float headerH = std::max(38.0f, height_ * 0.105f);
    fillRect({0.0f, 0.0f, static_cast<float>(width_), headerH}, kTop);

    for (int i = 0; i < 6; ++i) {
        const auto page = static_cast<NativePage>(i);
        fillRect(navRect(i), page == page_ ? kCyan : kButton);
    }

    if (page_ == NativePage::Play || page_ == NativePage::Drums) {
        fillRect(contentRect(), kPanel);
        drawGrid();
    } else {
        drawPlaceholder();
    }

    glDisable(GL_SCISSOR_TEST);
}

std::optional<NativePage> NativeUi::hitNav(float x, float y) const noexcept {
    for (int i = 0; i < 6; ++i) {
        if (navRect(i).contains(x, y)) {
            return static_cast<NativePage>(i);
        }
    }
    return std::nullopt;
}

std::optional<int> NativeUi::hitPitch(float x, float y) const noexcept {
    if (page_ != NativePage::Play && page_ != NativePage::Drums) {
        return std::nullopt;
    }

    for (int visualRow = 0; visualRow < 7; ++visualRow) {
        for (int column = 0; column < 7; ++column) {
            if (!gridRect(visualRow, column).contains(x, y)) continue;
            const int block = 6 - visualRow;
            return kGridLow + block * 7 + column;
        }
    }
    return std::nullopt;
}

int NativeUi::padIndexForMidi(int midi) noexcept {
    const int clamped = std::clamp(midi, kGridLow, kGridHigh);
    const int lowToHighBlock = (clamped - kGridLow) / 7;
    return 6 - lowToHighBlock;
}

} // namespace aiora
