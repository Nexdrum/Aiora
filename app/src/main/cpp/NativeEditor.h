#pragma once

#include <cstdint>
#include <optional>

#include "AioraTypes.h"
#include "ProjectCore.h"
#include "NativeUi.h"
#include "KineticScroll.h"

namespace aiora {

enum class EditorPage : uint8_t { Synth, Fx };
enum class PatchTransferAction : uint8_t { AiFromClipboard, CopyPatch, PastePatch };

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
    void setSafeInsets(int left,int top,int right,int bottom) noexcept;
    void renderSynth() const noexcept;
    void renderFx() const noexcept;

    bool pointerDown(EditorPage page, float x, float y);
    bool pointerMove(float x, float y);
    bool pointerUp();
    void cancel() noexcept;
    void stopKineticScrolling() noexcept { kineticScroll_.stop(); }
    void advanceKineticScrolling(float dt) noexcept;

    [[nodiscard]] std::optional<PatchTransferAction> hitPatchTransfer(
        EditorPage page, float x, float y) const noexcept;
    [[nodiscard]] std::optional<int> hitFactoryPreset(float x,float y) const noexcept;
    [[nodiscard]] std::optional<int> hitOperatorRatio(float x,float y) const noexcept;
    [[nodiscard]] std::optional<int> hitOperatorSemitone(float x,float y) const noexcept;
    struct ModLinkValueHit {
        int slot{-1};
        LinkSource source{LinkSource::Mod};
        bool maximum{false};
        ModTarget target{ModTarget::None};
        float value{0.0f};
    };
    [[nodiscard]] std::optional<ModLinkValueHit> hitModLinkValue(
        float x,float y) const noexcept;
    bool openDropdownAt(EditorPage page,float x,float y,NativeUi& ui) const;
    bool applyDropdownChoice(const DropdownChoice& choice);

private:
    NativeEditor() = default;

    enum class HitKind : uint8_t {
        None,
        SynthTab, OperatorSelect, WaveSelect, OperatorToggle,
        OperatorParam, Harmonic, Matrix,
        FxGroup, FilterType, FilterOpen, FilterEditor, UnisonEditor, PeakToggle, PeakMode, LfoTarget, PatchParam,
        LinkSourceV, LinkSourceM, ModTargetPrev, ModTargetNext, ModMin, ModMax, ModDelete, ModAdd,
        RackExpand, RackUp, RackDown, RackBypass, RackDelete, RackParameter, RackAdd
    };
    struct Hit {
        HitKind kind{HitKind::None};
        int a{-1};
        int b{-1};
        Rect rect{};
    };
    struct Range { float lo{},hi{}; };

    [[nodiscard]] Rect viewportRect() const noexcept;
    [[nodiscard]] Rect scrollGutterRect() const noexcept;
    [[nodiscard]] float scrollGutterPixels() const noexcept;
    [[nodiscard]] Rect contentRect() const noexcept;
    [[nodiscard]] Rect editorRect() const noexcept;
    [[nodiscard]] Rect bodyRect() const noexcept;
    [[nodiscard]] Rect patchTransferRect(EditorPage page,int index) const noexcept;

    [[nodiscard]] Rect factoryPresetRect(int index) const noexcept;
    [[nodiscard]] Rect synthTabRect(int index) const noexcept;
    [[nodiscard]] Rect operatorCardRect(int index) const noexcept;
    [[nodiscard]] Rect operatorWaveFieldRect(int op) const noexcept;
    [[nodiscard]] Rect operatorCardParamRect(int op,int param) const noexcept;
    [[nodiscard]] Rect operatorCardToggleRect(int op) const noexcept;
    [[nodiscard]] Rect operatorCardHarmonicRect(int op,int partial) const noexcept;
    [[nodiscard]] Rect operatorSelectRect(int index) const noexcept;
    [[nodiscard]] Rect waveRect(int index) const noexcept;
    [[nodiscard]] Rect operatorToggleRect() const noexcept;
    [[nodiscard]] Rect operatorSliderRect(int index,bool custom) const noexcept;
    [[nodiscard]] Rect harmonicRect(int index) const noexcept;
    [[nodiscard]] Rect matrixRect(int modulator,int carrier) const noexcept;

    [[nodiscard]] Rect fxWorkspaceTabRect(int index) const noexcept;
    [[nodiscard]] Rect lfoWorkspaceRow(int row) const noexcept;
    [[nodiscard]] Rect lfoWorkspaceTargetRect() const noexcept;
    [[nodiscard]] Rect fxRackRect() const noexcept;
    [[nodiscard]] Rect fxRackStageRect(int index) const noexcept;
    [[nodiscard]] Rect fxRackParamRect(int index,int parameter) const noexcept;
    [[nodiscard]] Rect fxRackAddRect(bool add) const noexcept;
    [[nodiscard]] Rect fxGroupRect(int index) const noexcept;
    [[nodiscard]] Rect fxSectionParamRect(int section,int row) const noexcept;
    [[nodiscard]] Rect filterTypeRect(int index) const noexcept;
    [[nodiscard]] Rect peakToggleRect(int index) const noexcept;
    [[nodiscard]] Rect peakParamRect(int row) const noexcept;
    [[nodiscard]] Rect lfoTargetRect(int index) const noexcept;
    [[nodiscard]] Rect fxSliderRect(int row,int rowCount,bool hasChoiceRow) const noexcept;
    [[nodiscard]] Rect linkSourceRect(LinkSource source) const noexcept;
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
    [[nodiscard]] float editorScrollMax(EditorPage page) const noexcept;
    void scrollEditor(EditorPage page,float delta) noexcept;
    bool applyKineticScrollDelta(float delta) noexcept;

    void fillRect(Rect rect,Rgb color) const noexcept;
    void drawScrollGutter() const noexcept;
    void drawSlider(Rect rect,float norm,Rgb accent) const noexcept;
    void drawButton(Rect rect,bool active,Rgb accent) const noexcept;
    void drawPadReservedBackground() const noexcept;
    void drawPatchTransfer(EditorPage page) const noexcept;

    int width_{0};
    int height_{0};
    int safeLeft_{0};
    int safeTop_{0};
    int safeRight_{0};
    int safeBottom_{0};
    int selectedOperator_{0};
    bool matrixMode_{false};
    int fxWorkspace_{0}; // Filter / Amp / Unison / LFO / Addable FX
    int fxGroup_{0};
    int fxRackExpanded_{-1};
    FxType fxRackNewType_{FxType::MetalAmp};
    LinkSource linkSource_{LinkSource::Mod};
    float synthScrollY_{0.0f};
    float fxScrollY_{0.0f};
    bool scrolling_{false};
    float scrollLastY_{0.0f};
    KineticScroll kineticScroll_{};

    std::optional<Hit> activeHit_;
    EditorPage activePage_{EditorPage::Synth};
    bool changed_{false};
};

} // namespace aiora
