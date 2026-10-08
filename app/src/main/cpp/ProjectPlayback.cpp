#include "ProjectCore.h"
#include "PlaybackSnapshot.h"
#include <algorithm>
#include <cmath>

namespace aiora {
namespace {
constexpr float kPi = 3.14159265358979323846f;

void pushPoint(DspCurve& out,float step,float value) noexcept {
    if(out.count>=out.points.size())return;
    if(out.count>0 && std::fabs(out.points[out.count-1].step-step)<1.0e-5f){out.points[out.count-1]={step,value};return;}
    out.points[out.count++]={step,value};
}

DspCurve bendCurve(const Note& note,const DrumPad* pad) {
    DspCurve out;
    const float L=std::max(1.0f,note.lengthSteps);
    pushPoint(out,0.0f,0.0f);
    auto pts=note.bend;
    std::sort(pts.begin(),pts.end(),[](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});
    for(const auto& p:pts){
        float value=std::clamp(p.value,-12.0f,12.0f);
        if(pad){
            const float lo=static_cast<float>(std::min(pad->lowMidi,pad->highMidi))-0.5f-static_cast<float>(note.midi);
            const float hi=static_cast<float>(std::max(pad->lowMidi,pad->highMidi))+0.5f-static_cast<float>(note.midi);
            value=std::clamp(value,lo,hi);
        }
        pushPoint(out,std::clamp(p.step+0.5f,0.0f,L),value);
    }
    const float last=out.count?out.points[out.count-1].value:0.0f;
    if(out.count==0 || std::fabs(out.points[out.count-1].step-L)>1.0e-5f)
        pushPoint(out,L,last);
    return out;
}

DspCurve levelCurve(const std::vector<CurvePoint>& source,float length) {
    DspCurve out;
    if(source.empty())return out;
    const float L=std::max(1.0f,length);
    auto pts=source;
    std::sort(pts.begin(),pts.end(),[](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});
    const float first=std::clamp(pts.front().value,0.0f,1.0f);
    pushPoint(out,0.0f,first);
    for(const auto& p:pts)pushPoint(out,std::clamp(p.step+0.5f,0.0f,L),std::clamp(p.value,0.0f,1.0f));
    const float last=out.count?out.points[out.count-1].value:first;
    if(out.count==0 || std::fabs(out.points[out.count-1].step-L)>1.0e-5f)pushPoint(out,L,last);
    return out;
}

int padIndexFor(const Track& track,int midi) {
    for(int i=0;i<static_cast<int>(track.pads.size());++i){
        const auto& pad=track.pads[static_cast<size_t>(i)];
        const int lo=std::min(pad.lowMidi,pad.highMidi),hi=std::max(pad.lowMidi,pad.highMidi);
        if(midi>=lo&&midi<=hi)return i;
    }
    return -1;
}

std::pair<float,float> panGain(float gain,float pan) {
    const float p=std::clamp(pan,-1.0f,1.0f);
    const float angle=(p+1.0f)*kPi*0.25f;
    return {gain*std::cos(angle),gain*std::sin(angle)};
}
}

std::unique_ptr<PlaybackSnapshot> ProjectCore::makePlaybackSnapshot() const {
    std::scoped_lock lock(mutex_);
    auto snap=std::make_unique<PlaybackSnapshot>();
    snap->bpm=project_.bpm;
    snap->divisions=std::max(1,project_.divisions);
    const int bar=std::max(1,project_.beats)*std::max(1,project_.divisions);
    float end=0.0f;
    bool anyNote=false;
    for(const auto& track:project_.tracks){
        for(const auto& note:track.notes){
            end=std::max(
                end,
                note.startStep+std::max(1.0f,note.lengthSteps));
            anyNote=true;
        }
    }
    snap->lengthSteps=anyNote
        ?static_cast<int>(std::ceil(end/static_cast<float>(bar)))*bar
        :bar;
    snap->lengthSteps=std::max(bar,snap->lengthSteps);
    snap->masterVolume=project_.masterVolume;
    snap->masterReverb=project_.masterReverb;
    snap->steps.resize(static_cast<size_t>(snap->lengthSteps));

    auto addFxBus=[&](const Fx& fx)->int {
        if(snap->fxBuses.size()>=kMaxPlaybackFxBuses)return -1;
        snap->fxBuses.push_back(fx);
        return static_cast<int>(snap->fxBuses.size())-1;
    };

    const bool anySolo=std::any_of(project_.tracks.begin(),project_.tracks.end(),[](const Track&t){return t.solo;});
    for(const auto& track:project_.tracks){
        if(track.mute || (anySolo&&!track.solo))continue;
        const int melodicBus=track.drums?-1:addFxBus(track.patch.fx);
        std::vector<int> padBuses(track.pads.size(),-2);

        for(const auto& note:track.notes){
            const int step=std::clamp(static_cast<int>(std::lround(note.startStep)),0,snap->lengthSteps-1);
            PlaybackEvent event;
            event.midi=note.midi;
            event.lengthSteps=std::max(1.0f,note.lengthSteps);

            const int padIndex=track.drums?padIndexFor(track,note.midi):-1;
            if(track.drums && padIndex<0)continue;
            if(padIndex>=0){
                const auto& pad=track.pads[static_cast<size_t>(padIndex)];
                if(padBuses[static_cast<size_t>(padIndex)]==-2)padBuses[static_cast<size_t>(padIndex)]=addFxBus(pad.patch.fx);
                event.fxBus=static_cast<int16_t>(padBuses[static_cast<size_t>(padIndex)]);
                event.patch=toDspPatch(pad.patch);
                event.automation.bend=bendCurve(note,&pad);
                const auto [l,r]=panGain(track.volume*pad.volume,std::clamp(track.pan+pad.pan,-1.0f,1.0f));
                event.gainLeft=l;event.gainRight=r;
            }else{
                event.fxBus=static_cast<int16_t>(melodicBus);
                event.patch=toDspPatch(track.patch);
                event.automation.bend=bendCurve(note,nullptr);
                const auto [l,r]=panGain(track.volume,track.pan);
                event.gainLeft=l;event.gainRight=r;
            }
            event.automation.velocity=levelCurve(note.velocity,note.lengthSteps);
            event.automation.mod=levelCurve(note.mod,note.lengthSteps);
            snap->steps[static_cast<size_t>(step)].events.push_back(std::move(event));
        }
    }
    return snap;
}

} // namespace aiora
