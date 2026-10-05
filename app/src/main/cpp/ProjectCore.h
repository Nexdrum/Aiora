#pragma once
#include <mutex>
#include <string>
#include "AioraTypes.h"

namespace aiora {

class ProjectCore {
public:
    static ProjectCore& instance();

    void reset();
    int addTrack(bool drums = false);
    bool deleteTrack(int index);
    bool selectTrack(int index);
    int selectedTrack() const;
    int trackCount() const;

    std::string trackName(int index) const;
    void setTrackName(int index, const std::string& name);
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
    int padCount(int trackIndex) const;
    int selectedPad(int trackIndex) const;
    bool selectPad(int trackIndex, int padIndex);
    std::string padIcon(int trackIndex, int padIndex) const;
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

    DspPatch selectedDspPatch() const;
    Fx selectedFx() const;
    DspPatch padDspPatch(int trackIndex, int padIndex) const;
    Fx padFx(int trackIndex, int padIndex) const;

private:
    ProjectCore() = default;
    static std::string autoName(int index);
    bool validTrack(int index) const noexcept;
    bool validPad(int trackIndex, int padIndex) const noexcept;
    bool validNote(int trackIndex, int noteIndex) const noexcept;
    bool rangeFree(int trackIndex, int padIndex, int low, int high) const noexcept;
    bool pitchAllowed(int trackIndex, int midi) const noexcept;

    mutable std::mutex mutex_;
    Project project_{};
    int selectedTrack_{-1};
};

} // namespace aiora
