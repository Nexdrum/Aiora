#pragma once
#include <memory>
#include <mutex>
#include <string>
#include "AioraTypes.h"

namespace aiora {

struct PlaybackSnapshot;

enum class OperatorParam : uint8_t {
    Ratio, Detune, Level, Attack, Decay, Sustain, Release
};

enum class PatchParam : uint8_t {
    FilterCutoff, FilterResonance, FilterEnv,
    FilterAttack, FilterDecay, FilterSustain, FilterRelease,
    AmpAttack, AmpDecay, AmpSustain, AmpRelease,
    VelocityAmp, VelocityFilter,
    LfoRate, LfoAmount, LfoAttack, LfoVelocity,
    Unison, Glide, Volume,
    Distortion, Delay, DelayTime, DelayFeedback, Reverb
};

class ProjectCore {
public:
    static ProjectCore& instance();

    void reset();
    Project projectCopy() const;
    bool replaceProject(Project project, int selectedTrack = 0);
    bool replaceSelectedPatch(Patch patch);
    int addTrack(bool drums = false);
    bool deleteTrack(int index);
    bool selectTrack(int index);
    int selectedTrack() const;
    int trackCount() const;

    std::string trackName(int index) const;
    std::string trackPatchName(int index) const;
    void setTrackName(int index, const std::string& name);
    void clearTrackNotes(int index);
    bool trackIsDrums(int index) const;
    bool trackMute(int index) const;
    bool trackSolo(int index) const;
    float trackVolume(int index) const;
    float trackPan(int index) const;
    void setTrackMute(int index, bool value);
    void setTrackSolo(int index, bool value);
    void setTrackVolume(int index, float value);
    void setTrackPan(int index, float value);

    bool loadNexdrumKit(int trackIndex);
    int addDrumPad(int trackIndex);
    bool deleteDrumPad(int trackIndex, int padIndex);
    bool setPadCenter(int trackIndex, int padIndex, int midi);
    bool setPadIcon(int trackIndex, int padIndex, const std::string& icon);
    int padCount(int trackIndex) const;
    int selectedPad(int trackIndex) const;
    bool selectPad(int trackIndex, int padIndex);
    std::string padIcon(int trackIndex, int padIndex) const;
    std::string padName(int trackIndex, int padIndex) const;
    void setPadName(int trackIndex, int padIndex, const std::string& name);
    std::string padPatchName(int trackIndex, int padIndex) const;
    int padCenter(int trackIndex, int padIndex) const;
    int padLow(int trackIndex, int padIndex) const;
    int padHigh(int trackIndex, int padIndex) const;
    float padVolume(int trackIndex, int padIndex) const;
    float padPan(int trackIndex, int padIndex) const;
    void setPadVolume(int trackIndex, int padIndex, float value);
    void setPadPan(int trackIndex, int padIndex, float value);
    bool setPadRange(int trackIndex, int padIndex, int low, int high);

    int noteCount(int trackIndex) const;
    int addNote(int trackIndex, int midi, float startStep, float lengthSteps);
    int pasteNotes(int trackIndex, const std::vector<Note>& notes, float startStep);
    std::vector<int> duplicateNotes(
        int trackIndex,const std::vector<int>& noteIndices,float stepOffset);
    int deleteNotes(int trackIndex,const std::vector<int>& noteIndices);
    int deleteNotesInRange(int trackIndex,float startStep,float endStep);
    bool moveNotes(
        int trackIndex,const std::vector<int>& noteIndices,
        int midiDelta,float stepDelta);
    bool applyGroupAutomation(
        int trackIndex,const std::vector<int>& noteIndices,int curveKind,
        float groupStart,const std::vector<CurvePoint>& groupPoints);
    bool deleteNote(int trackIndex, int noteIndex);
    bool updateNote(int trackIndex, int noteIndex, int midi, float startStep, float lengthSteps);
    int noteMidi(int trackIndex, int noteIndex) const;
    float noteStart(int trackIndex, int noteIndex) const;
    float noteLength(int trackIndex, int noteIndex) const;
    int curvePointCount(int trackIndex, int noteIndex, int curveKind) const;
    float curvePointStep(int trackIndex, int noteIndex, int curveKind, int pointIndex) const;
    float curvePointValue(int trackIndex, int noteIndex, int curveKind, int pointIndex) const;
    bool curvePointFree(int trackIndex, int noteIndex, int curveKind, int pointIndex) const;
    int addCurvePoint(int trackIndex, int noteIndex, int curveKind, float step, float value, bool free);
    bool updateCurvePoint(int trackIndex, int noteIndex, int curveKind, int pointIndex, float step, float value, bool free);
    bool deleteCurvePoint(int trackIndex, int noteIndex, int curveKind, int pointIndex);
    float lastStep() const;
    int playLengthSteps() const;

    float bpm() const;
    int beats() const;
    int divisions() const;
    bool dozenal() const;
    float masterVolume() const;
    float masterReverb() const;
    void setBpm(float value);
    void setSignature(int beats, int divisions);
    void setDozenal(bool enabled);
    void setMasterVolume(float value);
    void setMasterReverb(float value);

    Patch selectedPatch() const;
    DspPatch selectedDspPatch() const;
    Fx selectedFx() const;
    DspPatch padDspPatch(int trackIndex, int padIndex) const;
    Fx padFx(int trackIndex, int padIndex) const;

    bool setSelectedOperatorEnabled(int opIndex, bool enabled);
    bool setSelectedOperatorWave(int opIndex, Wave wave);
    bool setSelectedOperatorParam(int opIndex, OperatorParam param, float value);
    bool setSelectedHarmonic(int opIndex, int partialIndex, float value, bool muted = false);
    bool setSelectedMatrixAmount(int modulator, int carrier, float value);
    bool setSelectedFilterType(FilterType type);
    bool setSelectedLfoTarget(LfoTarget target);
    bool setSelectedPatchParam(PatchParam param, float value);

    int selectedModSlotCount() const;
    ModSlot selectedModSlot(int slotIndex) const;
    bool addSelectedModSlot(ModTarget target = ModTarget::Cutoff);
    bool removeSelectedModSlot(int slotIndex);
    bool setSelectedModSlot(int slotIndex, ModTarget target, float minValue, float maxValue);

    std::unique_ptr<PlaybackSnapshot> makePlaybackSnapshot() const;

private:
    ProjectCore() = default;
    static std::string autoName(int index);
    bool validTrack(int index) const noexcept;
    bool validPad(int trackIndex, int padIndex) const noexcept;
    bool validNote(int trackIndex, int noteIndex) const noexcept;
    bool rangeFree(int trackIndex, int padIndex, int low, int high) const noexcept;
    bool pitchAllowed(int trackIndex, int midi) const noexcept;
    Patch* selectedPatchUnsafe() noexcept;
    const Patch* selectedPatchUnsafe() const noexcept;

    mutable std::mutex mutex_;
    Project project_{};
    int selectedTrack_{-1};
};

} // namespace aiora
