#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace aiora {

enum class NativePage : int {
    Tracks = 0,
    Drums = 1,
    Roll = 2,
    Synth = 3,
    Fx = 4,
    Play = 5,
    Performance = 6
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

enum class RollSelectionTool : int {
    Pencil = 0,
    Lasso = 1
};

struct RollLassoPoint {
    float x{0.0f};
    float y{0.0f};
};

struct RollGroupPoint {
    float step{0.0f};
    float value{0.0f};
};

enum class RollCornerAction : int {
    Copy,
    Paste
};

enum class HeaderAction : int {
    BpmDown, BpmUp,
    BeatsDown, BeatsUp,
    DivDown, DivUp,
    DozenalToggle,
    TransportStart,
    TransportToggle,
    TransportEnd,
    PerformanceView
};

enum class ProjectTransferAction : int {
    ExportWav,
    ExportMidi
};

enum class TrackSwitchAction : int {
    PreviousTrack,
    NextTrack,
    PreviousPad,
    NextPad
};

enum class DropdownKind : int {
    None = 0,
    Track,
    Pad,
    Patch,
    Wave,
    FilterType,
    LfoTarget,
    ModTarget
};

struct DropdownChoice {
    DropdownKind kind{DropdownKind::None};
    int context{-1};
    int option{-1};
};

enum class TrackUtilityAction : int {
    DozenalToggle,
    AiFromClipboard,
    ClearTrack,
    NewProject,
    SaveProject,
    LoadProject
};

struct RollCellHit {
    int midi{-1};
    int step{-1};
    float normalizedAcross{0.5f};
    float normalizedDown{0.5f};
};

struct RollNoteHit {
    int noteIndex{-1};
    bool tail{false};
};

struct RollAutomationHit {
    int noteIndex{-1};
    int pointIndex{-1};
    float step{0.0f};
    float value{0.0f};
    bool free{false};
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
    void setSafeInsets(int left,int top,int right,int bottom) noexcept;
    void setPage(NativePage page) noexcept { page_ = page; closeDropdown(); }
    [[nodiscard]] NativePage page() const noexcept { return page_; }
    void togglePerformancePage() noexcept;
    void setPerformanceFollow(bool enabled) noexcept { performanceFollow_ = enabled; }

    void setPitchActive(int midi, bool active) noexcept;
    void clearPitchActivity() noexcept;

    bool trackPointerDown(float x, float y);
    bool trackPointerMove(float x, float y);
    bool trackPointerUp();

    bool drumPointerDown(float x, float y);
    bool drumPointerMove(float x, float y);
    bool drumPointerUp();
    bool drumPitchTap(int midi);
    void resetDrumRangeArm() noexcept { drumRangeArmed_ = false; }
    [[nodiscard]] bool drumRangeMode() const noexcept;

    void setRollMode(RollMode mode) noexcept { rollMode_ = mode; }
    [[nodiscard]] RollMode rollMode() const noexcept { return rollMode_; }

    void setRollSelectionTool(RollSelectionTool tool) noexcept { rollSelectionTool_=tool; }
    [[nodiscard]] RollSelectionTool rollSelectionTool() const noexcept { return rollSelectionTool_; }
    void toggleRollSelectionTool() noexcept {
        rollSelectionTool_=rollSelectionTool_==RollSelectionTool::Pencil
            ?RollSelectionTool::Lasso:RollSelectionTool::Pencil;
    }
    void setRollMultiLasso(bool enabled) noexcept { rollMultiLasso_=enabled; }
    [[nodiscard]] bool rollMultiLasso() const noexcept { return rollMultiLasso_; }
    void toggleRollMultiLasso() noexcept { rollMultiLasso_=!rollMultiLasso_; }

    void setRollNoteSelection(int track,std::vector<int> indices);
    void clearRollNoteSelection() noexcept;
    [[nodiscard]] bool rollNoteSelectionActive() const noexcept {
        return rollSelectedTrack_>=0&&!rollSelectedNotes_.empty();
    }
    [[nodiscard]] int rollSelectedTrack() const noexcept { return rollSelectedTrack_; }
    [[nodiscard]] const std::vector<int>& rollSelectedNotes() const noexcept {
        return rollSelectedNotes_;
    }
    [[nodiscard]] bool rollNoteSelected(int noteIndex) const noexcept;

    void beginRollLasso(float x,float y);
    void appendRollLasso(float x,float y);
    std::vector<int> finishRollLasso();
    void cancelRollLasso() noexcept { rollLassoPath_.clear(); }

    void setRollGroupAutomation(
        int kind,float start,float end,std::vector<RollGroupPoint> points);
    void clearRollGroupAutomation() noexcept;
    [[nodiscard]] bool hitRollGroupAutomationGutter(float x,float y) const noexcept;
    [[nodiscard]] std::optional<int> hitRollGroupAutomationPoint(
        float x,float y) const noexcept;
    [[nodiscard]] bool rollGroupAutomationPosition(
        float x,float y,float& step,float& value) const noexcept;

    void setRollRangeAllTracks(bool enabled) noexcept { rollRangeAllTracks_=enabled; }
    [[nodiscard]] bool rollRangeAllTracks() const noexcept { return rollRangeAllTracks_; }
    [[nodiscard]] bool rollHasAnySelection() const noexcept {
        return rollNoteSelectionActive()||
            (rollSelectionActive_&&rollSelectionAnchorStep_!=rollSelectionEndStep_);
    }

    void scrollRoll(int pitchDelta, int stepDelta) noexcept;
    void setRollStartStep(int step) noexcept;
    [[nodiscard]] int rollStartStep() const noexcept { return rollStartStep_; }
    void setRollSelection(bool active,int anchorStep,int endStep) noexcept;
    [[nodiscard]] bool rollSelectionActive() const noexcept { return rollSelectionActive_; }
    [[nodiscard]] int rollSelectionAnchorStep() const noexcept { return rollSelectionAnchorStep_; }
    [[nodiscard]] int rollSelectionEndStep() const noexcept { return rollSelectionEndStep_; }
    void setRollClipboardAvailable(bool available) noexcept { rollClipboardAvailable_=available; }
    void scrollPage(float deltaPixels) noexcept;
    [[nodiscard]] bool hitScrollableBody(float x,float y) const noexcept;
    [[nodiscard]] float rollCellPixels() const noexcept;
    [[nodiscard]] float rollColumnPixels() const noexcept;

    void render() const noexcept;

    void openDropdown(
        DropdownKind kind,int context,Rect anchor,int selected,
        std::vector<std::string> labels);
    void closeDropdown() noexcept;
    [[nodiscard]] bool dropdownOpen() const noexcept { return dropdownKind_!=DropdownKind::None; }
    [[nodiscard]] bool openUiDropdownAt(float x,float y);
    [[nodiscard]] std::optional<DropdownChoice> hitDropdown(float x,float y);

    [[nodiscard]] std::optional<HeaderAction> hitHeader(float x, float y) const noexcept;
    [[nodiscard]] std::optional<NativePage> hitNav(float x, float y) const noexcept;
    [[nodiscard]] std::optional<TrackSwitchAction> hitTrackSwitch(float x,float y) const noexcept;
    [[nodiscard]] std::optional<int> hitPitch(float x, float y) const noexcept;
    [[nodiscard]] std::optional<int> hitTrack(float x, float y) const noexcept;
    [[nodiscard]] std::optional<int> hitTrackName(float x,float y) const noexcept;
    [[nodiscard]] std::optional<TrackUtilityAction> hitTrackUtility(float x,float y) const noexcept;
    [[nodiscard]] std::optional<ProjectTransferAction> hitProjectTransfer(float x, float y) const noexcept;
    [[nodiscard]] std::optional<TrackAddKind> hitAddTrack(float x, float y) const noexcept;
    [[nodiscard]] std::optional<int> hitPadQuick(float x, float y) const noexcept;
    [[nodiscard]] std::optional<int> hitPadName(float x,float y) const noexcept;
    [[nodiscard]] std::optional<RollMode> hitRollMode(float x, float y) const noexcept;
    [[nodiscard]] bool hitRollSelectionTool(float x,float y) const noexcept;
    [[nodiscard]] bool hitRollRangeScopeToggle(float x,float y) const noexcept;
    [[nodiscard]] bool hitRollDelete(float x,float y) const noexcept;
    [[nodiscard]] bool hitRollPitchHeader(float x,float y) const noexcept;
    [[nodiscard]] std::optional<int> hitRollPitchHeaderMidi(float x,float y) const noexcept;
    [[nodiscard]] bool hitRollBeatGutter(float x,float y) const noexcept;
    [[nodiscard]] std::optional<int> hitRollBeatStep(float x,float y) const noexcept;
    [[nodiscard]] std::optional<RollCornerAction> hitRollCornerAction(float x,float y) const noexcept;
    [[nodiscard]] bool hitRollNoteArea(float x,float y) const noexcept;
    [[nodiscard]] std::optional<RollNoteHit> hitRollNote(float x,float y) const noexcept;
    [[nodiscard]] std::optional<RollCellHit> hitRollCell(float x, float y) const noexcept;
    [[nodiscard]] std::optional<RollAutomationHit> hitRollAutomation(
        float x,float y,int curveKind) const noexcept;
    [[nodiscard]] bool rollAutomationPosition(
        float x,float y,int noteIndex,int curveKind,bool free,
        float& step,float& value) const noexcept;

    [[nodiscard]] static int padIndexForMidi(int midi) noexcept;

private:
    [[nodiscard]] Rect headerScopeRect() const noexcept;
    [[nodiscard]] Rect headerPerformanceRect() const noexcept;
    [[nodiscard]] Rect headerControlRect(int index) const noexcept;
    [[nodiscard]] Rect navRect(int index) const noexcept;
    [[nodiscard]] Rect contentRect() const noexcept;
    [[nodiscard]] Rect bodyContentRect() const noexcept;
    [[nodiscard]] Rect pageScrollViewportRect() const noexcept;
    [[nodiscard]] Rect pageScrollGutterRect() const noexcept;
    [[nodiscard]] Rect pageScrollContentRect() const noexcept;
    [[nodiscard]] Rect trackSwitchRect(int part) const noexcept;
    [[nodiscard]] Rect gridAreaRect() const noexcept;
    [[nodiscard]] Rect gridRect(int visualRow, int column) const noexcept;
    [[nodiscard]] Rect trackCardRect() const noexcept;
    [[nodiscard]] Rect patchCardRect() const noexcept;
    [[nodiscard]] Rect patchPresetRect() const noexcept;
    [[nodiscard]] Rect aiCardRect() const noexcept;
    [[nodiscard]] Rect songCardRect() const noexcept;
    [[nodiscard]] Rect masterCardRect() const noexcept;
    [[nodiscard]] Rect trackSongSliderRect(int index) const noexcept;
    [[nodiscard]] Rect trackUtilityRect(int index) const noexcept;
    [[nodiscard]] Rect projectTransferRect(int index) const noexcept;
    [[nodiscard]] Rect trackRect(int index, int count) const noexcept;
    [[nodiscard]] Rect trackPartRect(int index, int count, int part) const noexcept;
    [[nodiscard]] Rect addTrackRect(TrackAddKind kind) const noexcept;
    [[nodiscard]] Rect masterSliderRect(int index) const noexcept;
    [[nodiscard]] Rect padQuickRect(int index, int count) const noexcept;
    [[nodiscard]] Rect drumKitCardRect() const noexcept;
    [[nodiscard]] Rect drumPadCardRect() const noexcept;
    [[nodiscard]] Rect drumEditorRect() const noexcept;
    [[nodiscard]] Rect drumActionRect(int index) const noexcept;
    [[nodiscard]] Rect drumSliderRect(int index) const noexcept;
    [[nodiscard]] Rect drumIconRect(int index) const noexcept;

    [[nodiscard]] Rect rollSelectionToolRect() const noexcept;
    [[nodiscard]] Rect rollModeRect(int index) const noexcept;
    [[nodiscard]] Rect rollGroupAutomationRect() const noexcept;
    [[nodiscard]] Rect rollViewportRect() const noexcept;
    [[nodiscard]] float rollGutterPixels() const noexcept;
    [[nodiscard]] float rollHeaderPixels() const noexcept;
    [[nodiscard]] std::vector<int> rollColumns() const;
    [[nodiscard]] int rollTotalRows() const noexcept;
    [[nodiscard]] float performanceHeaderPixels() const noexcept;
    [[nodiscard]] float performanceRowPixels() const noexcept;
    [[nodiscard]] float performanceStartStep() const noexcept;
    [[nodiscard]] Rgb pitchColor(int midi) const noexcept;

    void fillRect(const Rect& rect, Rgb color) const noexcept;
    void drawScope() const noexcept;
    void drawTrackSwitchBar() const noexcept;
    void drawPageScrollGutter() const noexcept;
    void drawGrid() const noexcept;
    void drawTracks() const noexcept;
    void drawPadQuick() const noexcept;
    void drawDrumEditor() const noexcept;
    void drawDrums() const noexcept;
    void drawRoll() const noexcept;
    void drawPerformance() const noexcept;
    void drawPlaceholder() const noexcept;
    void drawDropdown() const noexcept;
    [[nodiscard]] Rect dropdownPanelRect() const noexcept;
    [[nodiscard]] Rect dropdownItemRect(int index) const noexcept;
    [[nodiscard]] int dropdownColumns() const noexcept;

    int width_{0};
    int height_{0};
    int safeLeft_{0};
    int safeTop_{0};
    int safeRight_{0};
    int safeBottom_{0};
    NativePage page_{NativePage::Play};
    std::array<bool, kGridNotes> active_{};

    int trackActiveSlider_{-1};
    int trackActiveIndex_{-1};
    bool trackControlChanged_{false};

    bool drumRangeArmed_{false};
    int drumActiveSlider_{-1};
    bool drumControlChanged_{false};

    RollMode rollMode_{RollMode::Notes};
    RollSelectionTool rollSelectionTool_{RollSelectionTool::Pencil};
    bool rollMultiLasso_{false};
    int rollSelectedTrack_{-1};
    std::vector<int> rollSelectedNotes_{};
    std::vector<RollLassoPoint> rollLassoPath_{};
    int rollGroupAutomationKind_{-1};
    float rollGroupAutomationStart_{0.0f};
    float rollGroupAutomationEnd_{0.0f};
    std::vector<RollGroupPoint> rollGroupAutomationPoints_{};
    bool rollRangeAllTracks_{false};
    int rollPitchOffset_{0};
    int rollStepOffset_{0};
    int rollStartStep_{0};
    bool rollSelectionActive_{false};
    int rollSelectionAnchorStep_{0};
    int rollSelectionEndStep_{0};
    bool rollClipboardAvailable_{false};
    float trackScrollY_{0.0f};
    float drumScrollY_{0.0f};
    NativePage performanceReturnPage_{NativePage::Roll};
    bool performanceFollow_{true};
    float performanceScrollStep_{0.0f};

    DropdownKind dropdownKind_{DropdownKind::None};
    int dropdownContext_{-1};
    int dropdownSelected_{-1};
    Rect dropdownAnchor_{};
    std::vector<std::string> dropdownLabels_{};
};

} // namespace aiora
