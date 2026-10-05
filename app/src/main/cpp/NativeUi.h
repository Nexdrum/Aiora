#pragma once

#include <array>
#include <optional>

namespace aiora {

enum class NativePage : int {
    Tracks = 0,
    Drums = 1,
    Roll = 2,
    Synth = 3,
    Fx = 4,
    Play = 5
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

    void render() const noexcept;

    [[nodiscard]] std::optional<NativePage> hitNav(float x, float y) const noexcept;
    [[nodiscard]] std::optional<int> hitPitch(float x, float y) const noexcept;

    [[nodiscard]] static int padIndexForMidi(int midi) noexcept;

private:
    [[nodiscard]] Rect navRect(int index) const noexcept;
    [[nodiscard]] Rect gridRect(int visualRow, int column) const noexcept;
    [[nodiscard]] Rect contentRect() const noexcept;
    [[nodiscard]] Rgb pitchColor(int midi) const noexcept;

    void fillRect(const Rect& rect, Rgb color) const noexcept;
    void drawGrid() const noexcept;
    void drawPlaceholder() const noexcept;

    int width_{0};
    int height_{0};
    NativePage page_{NativePage::Play};
    std::array<bool, kGridNotes> active_{};
};

} // namespace aiora
