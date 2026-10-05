#include "NativeUi.h"

#include <GLES3/gl3.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "ProjectCore.h"

namespace aiora {
namespace {

constexpr NativeUi::Rgb kBg{0.0627f, 0.0706f, 0.0863f};
constexpr NativeUi::Rgb kTop{0.0784f, 0.0902f, 0.1137f};
constexpr NativeUi::Rgb kPanel{0.0863f, 0.1020f, 0.1294f};
constexpr NativeUi::Rgb kRollBg{0.0471f, 0.0549f, 0.0706f};
constexpr NativeUi::Rgb kCell{0.1020f, 0.1216f, 0.1608f};
constexpr NativeUi::Rgb kBeat{0.1255f, 0.1490f, 0.2039f};
constexpr NativeUi::Rgb kBar{0.1412f, 0.1725f, 0.2275f};
constexpr NativeUi::Rgb kButton{0.1373f, 0.1569f, 0.2000f};
constexpr NativeUi::Rgb kCyan{0.0f, 0.80f, 0.80f};
constexpr NativeUi::Rgb kOrange{1.0f, 0.6667f, 0.0f};
constexpr NativeUi::Rgb kWhite{0.91f, 0.925f, 0.945f};
constexpr NativeUi::Rgb kPurple{0.788f, 0.557f, 1.0f};
constexpr NativeUi::Rgb kMuted{0.35f, 0.39f, 0.47f};

constexpr std::array<NativeUi::Rgb, 12> kPitchColors{{
    {0.2275f, 1.0000f, 0.0000f},
    {0.0000f, 1.0000f, 0.9255f},
    {0.0000f, 0.5608f, 1.0000f},
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

bool pageHasPadQuick(NativePage page) noexcept {
    return page == NativePage::Drums ||
           page == NativePage::Synth ||
           page == NativePage::Fx;
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
    const auto nav = navRect(0);
    const float gap = std::max(4.0f, height_ * 0.010f);
    const float top = nav.y + nav.h + gap;
    return {
        margin,
        top,
        std::max(0.0f, static_cast<float>(width_) - margin * 2.0f),
        std::max(0.0f, static_cast<float>(height_) - top - margin)
    };
}

NativeUi::Rect NativeUi::gridAreaRect() const noexcept {
    auto area = contentRect();
    if (page_ == NativePage::Drums) {
        const float gap = std::max(4.0f, height_ * 0.010f);
        const float padH = std::clamp(area.h * 0.14f, 34.0f, 62.0f);
        area.y += padH + gap;
        area.h = std::max(0.0f, area.h - padH - gap);
    }
    return area;
}

NativeUi::Rect NativeUi::gridRect(int visualRow, int column) const noexcept {
    const auto area = gridAreaRect();
    const float gap = std::max(2.0f, std::min(width_, height_) * 0.006f);
    const float usableW = std::max(0.0f, area.w - gap * 6.0f);
    const float usableH = std::max(0.0f, area.h - gap * 6.0f);
    const float cell = std::max(1.0f, std::min(usableW / 7.0f, usableH / 7.0f));
    const float gridW = cell * 7.0f + gap * 6.0f;
    const float gridH = cell * 7.0f + gap * 6.0f;
    const float originX = area.x + (area.w - gridW) * 0.5f;
    const float originY = area.y + (area.h - gridH) * 0.5f;

    return {
        originX + column * (cell + gap),
        originY + visualRow * (cell + gap),
        cell,
        cell
    };
}

NativeUi::Rect NativeUi::addTrackRect(TrackAddKind kind) const noexcept {
    const auto content = contentRect();
    const float gap = std::max(5.0f, width_ * 0.008f);
    const float h = std::clamp(content.h * 0.13f, 38.0f, 58.0f);
    const float w = (content.w - gap * 3.0f) * 0.5f;
    const int side = kind == TrackAddKind::Drums ? 1 : 0;
    return {
        content.x + gap + side * (w + gap),
        content.y + content.h - h - gap,
        w,
        h
    };
}

NativeUi::Rect NativeUi::trackRect(int index, int count) const noexcept {
    const auto content = contentRect();
    const float gap = std::max(5.0f, height_ * 0.010f);
    const auto add = addTrackRect(TrackAddKind::Melodic);
    const float top = content.y + gap;
    const float bottom = add.y - gap;
    const float usable = std::max(0.0f, bottom - top);
    const int rows = std::max(1, count);
    const float rowH = std::min(70.0f, std::max(30.0f, (usable - gap * (rows - 1)) / rows));
    return {
        content.x + gap,
        top + index * (rowH + gap),
        std::max(0.0f, content.w - gap * 2.0f),
        rowH
    };
}

NativeUi::Rect NativeUi::padQuickRect(int index, int count) const noexcept {
    const auto content = contentRect();
    const int cells = std::max(1, count);
    const float gap = std::max(3.0f, width_ * 0.004f);
    const float h = std::clamp(content.h * 0.14f, 34.0f, 62.0f);
    const float available = std::max(0.0f, content.w - gap * (cells + 1));
    const float w = available / cells;
    return {
        content.x + gap + index * (w + gap),
        content.y,
        w,
        h
    };
}

NativeUi::Rect NativeUi::rollModeRect(int index) const noexcept {
    const auto content = contentRect();
    const float gap = std::max(3.0f, width_ * 0.004f);
    const float h = std::clamp(content.h * 0.11f, 34.0f, 48.0f);
    const float available = std::max(0.0f, content.w - gap * 5.0f);
    const float w = available / 4.0f;
    return {
        content.x + gap + index * (w + gap),
        content.y,
        w,
        h
    };
}

NativeUi::Rect NativeUi::rollViewportRect() const noexcept {
    auto content = contentRect();
    const float gap = std::max(4.0f, height_ * 0.010f);
    const float modeH = rollModeRect(0).h;
    content.y += modeH + gap;
    content.h = std::max(0.0f, content.h - modeH - gap);
    return content;
}

float NativeUi::rollCellPixels() const noexcept {
    const auto viewport = rollViewportRect();
    const float gutter = rollGutterPixels();
    const float header = rollHeaderPixels();
    const float byWidth = std::max(1.0f, (viewport.w - gutter) / 9.0f);
    const float byHeight = std::max(1.0f, (viewport.h - header) / 10.0f);
    return std::clamp(std::min(byWidth, byHeight), 22.0f, 36.0f);
}

float NativeUi::rollGutterPixels() const noexcept {
    return std::clamp(width_ * 0.075f, 34.0f, 52.0f);
}

float NativeUi::rollHeaderPixels() const noexcept {
    return std::clamp(height_ * 0.075f, 32.0f, 48.0f);
}

std::vector<int> NativeUi::rollColumns() const {
    auto& project = ProjectCore::instance();
    const int track = project.selectedTrack();
    std::vector<int> columns;

    if (track >= 0 && project.trackIsDrums(track)) {
        const int pads = project.padCount(track);
        for (int p = 0; p < pads; ++p) {
            int lo = project.padLow(track, p);
            int hi = project.padHigh(track, p);
            if (lo > hi) std::swap(lo, hi);
            if (lo < 0 || hi < 0) continue;
            for (int midi = lo; midi <= hi; ++midi) columns.push_back(midi);
        }
        std::sort(columns.begin(), columns.end());
        columns.erase(std::unique(columns.begin(), columns.end()), columns.end());
    }

    if (columns.empty()) {
        columns.reserve(97);
        for (int midi = 14; midi <= 110; ++midi) columns.push_back(midi);
    }
    return columns;
}

int NativeUi::rollTotalRows() const noexcept {
    return std::max(64, ProjectCore::instance().playLengthSteps() + 32);
}

void NativeUi::scrollRoll(int pitchDelta, int stepDelta) noexcept {
    const auto columns = rollColumns();
    const auto viewport = rollViewportRect();
    const float cell = rollCellPixels();
    const int visibleCols = std::max(
        1, static_cast<int>(std::floor((viewport.w - rollGutterPixels()) / cell)));
    const int visibleRows = std::max(
        1, static_cast<int>(std::floor((viewport.h - rollHeaderPixels()) / cell)));

    const int maxPitch = std::max(0, static_cast<int>(columns.size()) - visibleCols);
    const int maxStep = std::max(0, rollTotalRows() - visibleRows);
    rollPitchOffset_ = std::clamp(rollPitchOffset_ + pitchDelta, 0, maxPitch);
    rollStepOffset_ = std::clamp(rollStepOffset_ + stepDelta, 0, maxStep);
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
    int selectedLow = -1;
    int selectedHigh = -1;

    if (page_ == NativePage::Drums) {
        auto& project = ProjectCore::instance();
        const int track = project.selectedTrack();
        if (track >= 0 && project.trackIsDrums(track)) {
            const int pad = project.selectedPad(track);
            if (pad >= 0) {
                selectedLow = project.padLow(track, pad);
                selectedHigh = project.padHigh(track, pad);
                if (selectedLow > selectedHigh) std::swap(selectedLow, selectedHigh);
            }
        }
    }

    for (int visualRow = 0; visualRow < 7; ++visualRow) {
        const int block = 6 - visualRow;
        for (int column = 0; column < 7; ++column) {
            const int midi = kGridLow + block * 7 + column;
            const bool active = active_[static_cast<size_t>(midi - kGridLow)];
            const bool selected =
                selectedLow >= 0 && midi >= selectedLow && midi <= selectedHigh;
            const auto color = pitchColor(midi);
            const auto rect = gridRect(visualRow, column);

            const auto borderColor = selected
                ? mix(color, kOrange, 0.58f)
                : color;
            fillRect(rect, mix(kBg, borderColor, active ? 0.98f : (selected ? 0.86f : 0.62f)));

            const float border = std::max(2.0f, rect.w * (selected ? 0.075f : 0.055f));
            Rect inner{
                rect.x + border,
                rect.y + border,
                std::max(0.0f, rect.w - border * 2.0f),
                std::max(0.0f, rect.h - border * 2.0f)
            };
            fillRect(inner, mix(kPanel, color, active ? 0.52f : (selected ? 0.30f : 0.16f)));
        }
    }
}

void NativeUi::drawTracks() const noexcept {
    auto& project = ProjectCore::instance();
    const auto content = contentRect();
    fillRect(content, kPanel);

    const int count = project.trackCount();
    const int selected = project.selectedTrack();
    for (int i = 0; i < count; ++i) {
        const auto rect = trackRect(i, count);
        const bool drum = project.trackIsDrums(i);
        const bool isSelected = i == selected;
        const auto edge = isSelected ? kCyan : (drum ? mix(kButton, kOrange, 0.35f) : kButton);
        fillRect(rect, edge);

        const float inset = std::max(2.0f, rect.h * 0.055f);
        Rect inner{
            rect.x + inset,
            rect.y + inset,
            std::max(0.0f, rect.w - inset * 2.0f),
            std::max(0.0f, rect.h - inset * 2.0f)
        };
        fillRect(inner, isSelected ? mix(kPanel, kCyan, 0.12f) : kPanel);

        const float markerW = std::max(8.0f, rect.w * 0.025f);
        fillRect(
            {rect.x + rect.w - markerW - inset, rect.y + inset, markerW, rect.h - inset * 2.0f},
            drum ? kOrange : kCyan);

        if (project.trackMute(i)) {
            fillRect({rect.x + inset, rect.y + inset, markerW, rect.h - inset * 2.0f}, kMuted);
        }
        if (project.trackSolo(i)) {
            fillRect({rect.x + inset * 2.0f + markerW, rect.y + inset, markerW, rect.h - inset * 2.0f}, kOrange);
        }
    }

    fillRect(addTrackRect(TrackAddKind::Melodic), mix(kButton, kCyan, 0.28f));
    fillRect(addTrackRect(TrackAddKind::Drums), mix(kButton, kOrange, 0.32f));
}

void NativeUi::drawPadQuick() const noexcept {
    auto& project = ProjectCore::instance();
    const int track = project.selectedTrack();
    if (track < 0 || !project.trackIsDrums(track)) return;

    const int count = project.padCount(track);
    const int selected = project.selectedPad(track);
    for (int i = 0; i < count; ++i) {
        const auto rect = padQuickRect(i, count);
        const auto color = pitchColor(project.padCenter(track, i));
        fillRect(rect, i == selected ? kOrange : mix(kButton, color, 0.28f));

        const float inset = std::max(2.0f, rect.h * 0.075f);
        Rect inner{
            rect.x + inset,
            rect.y + inset,
            std::max(0.0f, rect.w - inset * 2.0f),
            std::max(0.0f, rect.h - inset * 2.0f)
        };
        fillRect(inner, i == selected ? mix(kPanel, kOrange, 0.22f) : kPanel);
    }
}

void NativeUi::drawDrums() const noexcept {
    fillRect(contentRect(), kPanel);
    drawPadQuick();
    drawGrid();
}

void NativeUi::drawRoll() const noexcept {
    auto& project = ProjectCore::instance();
    const auto content = contentRect();
    fillRect(content, kPanel);

    for (int i = 0; i < 4; ++i) {
        const auto mode = static_cast<RollMode>(i);
        const Rgb modeColor =
            mode == rollMode_
                ? (mode == RollMode::Bend ? kCyan :
                   mode == RollMode::Velocity ? kWhite :
                   mode == RollMode::Mod ? kPurple : kOrange)
                : kButton;
        fillRect(rollModeRect(i), modeColor);
    }

    const int track = project.selectedTrack();
    if (track < 0) return;

    const auto viewport = rollViewportRect();
    fillRect(viewport, kRollBg);

    const auto columns = rollColumns();
    const float cell = rollCellPixels();
    const float gutter = rollGutterPixels();
    const float header = rollHeaderPixels();
    const int visibleCols = std::max(
        1, static_cast<int>(std::floor((viewport.w - gutter) / cell)));
    const int visibleRows = std::max(
        1, static_cast<int>(std::floor((viewport.h - header) / cell)));

    const int pitchOffset = std::clamp(
        rollPitchOffset_, 0, std::max(0, static_cast<int>(columns.size()) - visibleCols));
    const int stepOffset = std::clamp(
        rollStepOffset_, 0, std::max(0, rollTotalRows() - visibleRows));

    fillRect({viewport.x, viewport.y, gutter - 1.0f, header - 1.0f}, kPanel);

    for (int c = 0; c < visibleCols && pitchOffset + c < static_cast<int>(columns.size()); ++c) {
        const int midi = columns[static_cast<size_t>(pitchOffset + c)];
        const float x = viewport.x + gutter + c * cell;
        fillRect({x, viewport.y, cell - 1.0f, header - 2.0f}, kPanel);
        fillRect({x, viewport.y + header - 4.0f, cell - 1.0f, 3.0f}, pitchColor(midi));
        if (((midi % 12) + 12) % 12 == 2) {
            fillRect({x + cell * 0.42f, viewport.y + 5.0f, cell * 0.16f, header * 0.35f}, kMuted);
        }
    }

    const int beatLen = std::max(1, project.divisions());
    const int barLen = std::max(1, project.beats()) * beatLen;

    for (int r = 0; r < visibleRows; ++r) {
        const int step = stepOffset + r;
        const float y = viewport.y + header + r * cell;
        const Rgb rowColor =
            step % barLen == 0 ? kBar :
            step % beatLen == 0 ? kBeat : kCell;

        fillRect({viewport.x, y, gutter - 2.0f, cell - 1.0f}, rowColor);
        for (int c = 0; c < visibleCols && pitchOffset + c < static_cast<int>(columns.size()); ++c) {
            const float x = viewport.x + gutter + c * cell;
            fillRect({x, y, cell - 1.0f, cell - 1.0f}, rowColor);
        }
    }

    const int notes = project.noteCount(track);
    for (int n = 0; n < notes; ++n) {
        const int midi = project.noteMidi(track, n);
        const auto it = std::lower_bound(columns.begin(), columns.end(), midi);
        if (it == columns.end() || *it != midi) continue;
        const int colIndex = static_cast<int>(std::distance(columns.begin(), it));
        const int visibleCol = colIndex - pitchOffset;
        if (visibleCol < 0 || visibleCol >= visibleCols) continue;

        const float start = project.noteStart(track, n);
        const float length = std::max(1.0f, project.noteLength(track, n));
        if (start + length <= stepOffset || start >= stepOffset + visibleRows) continue;

        const float x = viewport.x + gutter + visibleCol * cell + 1.0f;
        const float y = viewport.y + header + (start - stepOffset) * cell + 1.0f;
        const float h = std::max(cell, length * cell) - 2.0f;
        fillRect({x, y, cell - 3.0f, h}, pitchColor(midi));
        fillRect({x, y + h - 3.0f, cell - 3.0f, 3.0f}, kTop);

        if (rollMode_ == RollMode::Notes) continue;
        const int kind = static_cast<int>(rollMode_) - 1;
        const int points = project.curvePointCount(track, n, kind);
        const Rgb pointColor =
            rollMode_ == RollMode::Bend ? kCyan :
            rollMode_ == RollMode::Velocity ? kWhite : kPurple;

        for (int p = 0; p < points; ++p) {
            const float relStep = project.curvePointStep(track, n, kind, p);
            const float value = project.curvePointValue(track, n, kind, p);
            const bool free = project.curvePointFree(track, n, kind, p);
            const float py = viewport.y + header + (start + relStep + 0.5f - stepOffset) * cell;
            if (py < viewport.y + header || py > viewport.y + viewport.h) continue;

            if (rollMode_ == RollMode::Bend) {
                const float px = x + (cell - 3.0f) * 0.5f +
                    std::clamp(value / 12.0f, -1.0f, 1.0f) * (cell * 0.42f);
                const float s = free ? 7.0f : 5.0f;
                fillRect({px - s * 0.5f, py - s * 0.5f, s, s}, pointColor);
            } else {
                const float half = std::clamp(value, 0.0f, 1.0f) * (cell * 0.42f);
                const float center = x + (cell - 3.0f) * 0.5f;
                const float s = free ? 7.0f : 5.0f;
                fillRect({center - half - s * 0.5f, py - s * 0.5f, s, s}, pointColor);
                fillRect({center + half - s * 0.5f, py - s * 0.5f, s, s}, pointColor);
            }
        }
    }
}

void NativeUi::drawPlaceholder() const noexcept {
    const auto content = contentRect();
    fillRect(content, kPanel);

    const float gap = std::max(6.0f, content.w * 0.012f);
    const float topOffset = pageHasPadQuick(page_) ? std::clamp(content.h * 0.14f, 34.0f, 62.0f) + gap : 0.0f;
    const float rowH = std::max(28.0f, (content.h - topOffset) * 0.12f);
    for (int i = 0; i < 4; ++i) {
        Rect row{
            content.x + gap,
            content.y + topOffset + gap + i * (rowH + gap),
            std::max(0.0f, content.w - gap * 2.0f),
            rowH
        };
        fillRect(row, i == 0 ? mix(kButton, kCyan, 0.22f) : kButton);
    }

    if (pageHasPadQuick(page_)) drawPadQuick();
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

    switch (page_) {
        case NativePage::Tracks:
            drawTracks();
            break;
        case NativePage::Drums:
            drawDrums();
            break;
        case NativePage::Roll:
            drawRoll();
            break;
        case NativePage::Play:
            fillRect(contentRect(), kPanel);
            drawGrid();
            break;
        default:
            drawPlaceholder();
            break;
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

std::optional<int> NativeUi::hitTrack(float x, float y) const noexcept {
    if (page_ != NativePage::Tracks) return std::nullopt;
    const int count = ProjectCore::instance().trackCount();
    for (int i = 0; i < count; ++i) {
        if (trackRect(i, count).contains(x, y)) return i;
    }
    return std::nullopt;
}

std::optional<TrackAddKind> NativeUi::hitAddTrack(float x, float y) const noexcept {
    if (page_ != NativePage::Tracks) return std::nullopt;
    if (addTrackRect(TrackAddKind::Melodic).contains(x, y)) return TrackAddKind::Melodic;
    if (addTrackRect(TrackAddKind::Drums).contains(x, y)) return TrackAddKind::Drums;
    return std::nullopt;
}

std::optional<int> NativeUi::hitPadQuick(float x, float y) const noexcept {
    if (!pageHasPadQuick(page_)) return std::nullopt;

    auto& project = ProjectCore::instance();
    const int track = project.selectedTrack();
    if (track < 0 || !project.trackIsDrums(track)) return std::nullopt;

    const int count = project.padCount(track);
    for (int i = 0; i < count; ++i) {
        if (padQuickRect(i, count).contains(x, y)) return i;
    }
    return std::nullopt;
}

std::optional<RollMode> NativeUi::hitRollMode(float x, float y) const noexcept {
    if (page_ != NativePage::Roll) return std::nullopt;
    for (int i = 0; i < 4; ++i) {
        if (rollModeRect(i).contains(x, y)) return static_cast<RollMode>(i);
    }
    return std::nullopt;
}

std::optional<RollCellHit> NativeUi::hitRollCell(float x, float y) const noexcept {
    if (page_ != NativePage::Roll) return std::nullopt;

    const auto viewport = rollViewportRect();
    const float cell = rollCellPixels();
    const float gutter = rollGutterPixels();
    const float header = rollHeaderPixels();
    if (x < viewport.x + gutter || y < viewport.y + header ||
        x >= viewport.x + viewport.w || y >= viewport.y + viewport.h) {
        return std::nullopt;
    }

    const auto columns = rollColumns();
    const int visibleCol = static_cast<int>(std::floor((x - viewport.x - gutter) / cell));
    const int visibleRow = static_cast<int>(std::floor((y - viewport.y - header) / cell));
    const int colIndex = rollPitchOffset_ + visibleCol;
    const int step = rollStepOffset_ + visibleRow;
    if (visibleCol < 0 || visibleRow < 0 ||
        colIndex < 0 || colIndex >= static_cast<int>(columns.size()) ||
        step < 0 || step >= rollTotalRows()) {
        return std::nullopt;
    }

    const float cellLeft = viewport.x + gutter + visibleCol * cell;
    return RollCellHit{
        columns[static_cast<size_t>(colIndex)],
        step,
        std::clamp((x - cellLeft) / cell, 0.0f, 1.0f)
    };
}

int NativeUi::padIndexForMidi(int midi) noexcept {
    const int clamped = std::clamp(midi, kGridLow, kGridHigh);
    const int lowToHighBlock = (clamped - kGridLow) / 7;
    return 6 - lowToHighBlock;
}

} // namespace aiora
