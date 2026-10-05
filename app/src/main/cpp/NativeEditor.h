#pragma once

#include <cstdint>
#include <optional>

#include "AioraTypes.h"
#include "ProjectCore.h"

namespace aiora {

enum class EditorPage : uint8_t { Synth, Fx };

class NativeEditor {
public:
    struct Rect {
        float x{}, y{}, w{}, h{};
        [[nodiscard]] bool contains(float px,float py) const noexcept {
            return px>=x&&py>=y&&px<x+w&&py<y+h;
        }
    };
    struct Rgb { float r{},g{},b{}; };

    static NativeEditor& instance();

    void resize(int width, int height) noexcept;
    void renderSynth() const noexcept;
    void renderFx() const noexcept;

    bool pointerDown(EditorPage page, float x, float y);
    bool pointerMove(float x, float y);
    bool pointerUp();
    void cancel() noexcept;

private:
    NativeEditor() = default;

    enum class HitKind : uint8_t {
        None,
        SynthTab, OperatorSelect, WaveSelect, OperatorToggle,
        OperatorParam, Harmonic, Matrix,
        FxGroup, FilterType, LfoTarget, PatchParam,
        ModTargetPrev, ModTargetNext, ModMin, ModMax, ModDelete, ModAdd
    };
    struct Hit {
        HitKind kind{HitKind::None};
        int a{-1};
        int b{-1};
        Rect rect{};
    };
    struct Range { float lo{},hi{}; };

    [[nodiscard]] Rect contentRect() const noexcept;
    [[nodiscard]] Rect bodyRect() const noexcept;

    [[nodiscard]] Rect synthTabRect(int index) const noexcept;
    [[nodiscard]] Rect operatorSelectRect(int index) const noexcept;
    [[nodiscard]] Rect waveRect(int index) const noexcept;
    [[nodiscard]] Rect operatorToggleRect() const noexcept;
    [[nodiscard]] Rect operatorSliderRect(int index,bool custom) const noexcept;
    [[nodiscard]] Rect harmonicRect(int index) const noexcept;
    [[nodiscard]] Rect matrixRect(int modulator,int carrier) const noexcept;

    [[nodiscard]] Rect fxGroupRect(int index) const noexcept;
    [[nodiscard]] Rect filterTypeRect(int index) const noexcept;
    [[nodiscard]] Rect lfoTargetRect(int index) const noexcept;
    [[nodiscard]] Rect fxSliderRect(int row,int rowCount,bool hasChoiceRow) const noexcept;
    [[nodiscard]] Rect modRowRect(int slot) const noexcept;
    [[nodiscard]] Rect modPartRect(int slot,int part) const noexcept;
    [[nodiscard]] Rect modAddRect() const noexcept;

    [[nodiscard]] std::optional<Hit> hitSynth(float x,float y) const noexcept;
    [[nodiscard]] std::optional<Hit> hitFx(float x,float y) const noexcept;
    bool applyHit(const Hit& hit,float x,float y);

    [[nodiscard]] static Range operatorRange(OperatorParam param) noexcept;
    [[nodiscard]] static Range patchRange(PatchParam param) noexcept;
    [[nodiscard]] static Range modRange(ModTarget target) noexcept;
    [[nodiscard]] static float operatorValue(const Patch& p,int op,OperatorParam param) noexcept;
    [[nodiscard]] static float patchValue(const Patch& p,PatchParam param) noexcept;
    [[nodiscard]] static float normalized(float value,Range range) noexcept;
    [[nodiscard]] static float denormalized(float value,Range range) noexcept;
    [[nodiscard]] static ModTarget cycleModTarget(ModTarget current,int direction) noexcept;

    void fillRect(Rect rect,Rgb color) const noexcept;
    void drawSlider(Rect rect,float norm,Rgb accent) const noexcept;
    void drawButton(Rect rect,bool active,Rgb accent) const noexcept;
    void drawPadReservedBackground() const noexcept;

    int width_{0};
    int height_{0};
    int selectedOperator_{0};
    bool matrixMode_{false};
    int fxGroup_{0};

    std::optional<Hit> activeHit_;
    EditorPage activePage_{EditorPage::Synth};
    bool changed_{false};
};

} // namespace aiora
