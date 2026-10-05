#pragma once

#include <array>
#include <optional>
#include <vector>

namespace aiora {

enum class NativePage : int {
    Tracks = 0,
    Drums = 1,
    Roll = 2,
    Synth = 3,
    Fx = 4,
    Play = 5
};

enum class TrackAddKind : int {
    Melodic = 0,
    Drums = 1
};

enum class RollMode : int {
    Notes = 0,
    Bend = 1,
    Velocity = 2,
    Mod = 3
};

enum class HeaderAction : int {
    BpmDown, BpmUp,
    BeatsDown, BeatsUp,
    DivDown, DivUp,
    DozenalToggle,
    TransportToggle
};

struct RollCellHit {
    int midi{-1};
    int step{-1};
    float normalizedAcross{0.5f};
};

class NativeUi {
public:
    static constexpr int kGridLow = 38;
    static constexpr int kGridHigh = 86;
    static constexpr int kGridNotes = 49;

    struct Rect {
        float x{};
        float y{};
        float w{};
        float h{};

        [[nodiscard]] bool contains(float px, float py) const noexcept {
            return px >= x && py >= y && px < x + w && py < y + h;
        }
    };

    struct Rgb {
        float r{};
        float g{};
        float b{};
    };

    void resize(int width, int height) noexcept;
    void setPage(NativePage page) noexcept { page_ = page; }
    [[nodiscard]] NativePage page() const noexcept { return page_; }

    void setPitchActive(int midi, bool active) noexcept;
    void clearPitchActivity() noexcept;

    bool drumPointerDown(float x, float y);
    bool drumPointerMove(float x, float y);
    bool drumPointerUp();
    bool drumPitchTap(int midi);
    void resetDrumRangeArm() noexcept { drumRangeArmed_ = false; }
    [[nodiscard]] bool drumRangeMode() const noexcept;

    void setRollMode(RollMode mode) noexcept { rollMode_ = mode; }
    [[nodiscard]] RollMode rollMode() const noexcept { return rollMode_; }
    void scrollRoll(int pitchDelta, int stepDelta) noexcept;
    [[nodiscard]] float rollCellPixels() const noexcept;

    void render() const noexcept;

    [[nodiscard]] std::optional<HeaderAction> hitHeader(float x, float y) const noexcept;
    [[nodiscard]] std::optional<NativePage> hitNav(float x, float y) const noexcept;
    [[nodiscard]] std::optional<int> hitPitch(float x, float y) const noexcept;
    [[nodiscard]] std::optional<int> hitTrack(float x, float y) const noexcept;
    [[nodiscard]] std::optional<TrackAddKind> hitAddTrack(float x, float y) const noexcept;
    [[nodiscard]] std::optional<int> hitPadQuick(float x, float y) const noexcept;
    [[nodiscard]] std::optional<RollMode> hitRollMode(float x, float y) const noexcept;
    [[nodiscard]] std::optional<RollCellHit> hitRollCell(float x, float y) const noexcept;

    [[nodiscard]] static int padIndexForMidi(int midi) noexcept;

private:
    [[nodiscard]] Rect headerControlRect(int index) const noexcept;
    [[nodiscard]] Rect navRect(int index) const noexcept;
    [[nodiscard]] Rect contentRect() const noexcept;
    [[nodiscard]] Rect gridAreaRect() const noexcept;
    [[nodiscard]] Rect gridRect(int visualRow, int column) const noexcept;
    [[nodiscard]] Rect trackRect(int index, int count) const noexcept;
    [[nodiscard]] Rect addTrackRect(TrackAddKind kind) const noexcept;
    [[nodiscard]] Rect padQuickRect(int index, int count) const noexcept;
    [[nodiscard]] Rect drumEditorRect() const noexcept;
    [[nodiscard]] Rect drumActionRect(int index) const noexcept;
    [[nodiscard]] Rect drumSliderRect(int index) const noexcept;
    [[nodiscard]] Rect drumIconRect(int index) const noexcept;

    [[nodiscard]] Rect rollModeRect(int index) const noexcept;
    [[nodiscard]] Rect rollViewportRect() const noexcept;
    [[nodiscard]] float rollGutterPixels() const noexcept;
    [[nodiscard]] float rollHeaderPixels() const noexcept;
    [[nodiscard]] std::vector<int> rollColumns() const;
    [[nodiscard]] int rollTotalRows() const noexcept;
    [[nodiscard]] Rgb pitchColor(int midi) const noexcept;

    void fillRect(const Rect& rect, Rgb color) const noexcept;
    void drawGrid() const noexcept;
    void drawTracks() const noexcept;
    void drawPadQuick() const noexcept;
    void drawDrumEditor() const noexcept;
    void drawDrums() const noexcept;
    void drawRoll() const noexcept;
    void drawPlaceholder() const noexcept;

    int width_{0};
    int height_{0};
    NativePage page_{NativePage::Play};
    std::array<bool, kGridNotes> active_{};

    bool drumRangeArmed_{false};
    int drumActiveSlider_{-1};
    bool drumControlChanged_{false};

    RollMode rollMode_{RollMode::Notes};
    int rollPitchOffset_{0};
    int rollStepOffset_{0};
};

} // namespace aiora
