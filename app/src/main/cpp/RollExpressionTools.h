#pragma once

#include "RollCurveSimplify.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace aiora {

// These are offline editing helpers. No work here is performed in the audio callback.
// Curve evaluation deliberately uses the same cubic X-handle interpolator as playback.
inline float expressionCurveAt(
    const std::vector<CurvePoint>& curve,int kind,float step) noexcept {
    const float fallback=kind==1?1.0f:0.0f;
    if(curve.empty())return fallback;
    float previous=-0.5f;
    float previousValue=kind==0?0.0f:
        std::clamp(curve.front().value,0.0f,1.0f);
    for(const auto& point:curve){
        if(point.step<=previous+1.0e-6f){
            previous=point.step;
            previousValue=point.value;
            if(step<=previous+1.0e-6f)return previousValue;
            continue;
        }
        if(step<=point.step)return curveSegment(
            (step-previous)/(point.step-previous),
            previousValue,point.value,point.midpoint,
            point.handleActive,point.handleValue);
        previous=point.step;
        previousValue=point.value;
    }
    return previousValue;
}

inline const std::vector<CurvePoint>& expressionLane(
    const Note& note,int kind) noexcept {
    return kind==0?note.bend:kind==1?note.velocity:note.mod;
}

// A fixed musical-time sampling grid plus every original anchor and X handle
// captures fine vibrato and preserves shaped sections regardless of touch rate.
inline std::vector<float> expressionSampleTimes(
    const Note& note,int kind,const std::vector<float>& extra={}) {
    const float end=std::max(kMinNoteLengthSteps,note.lengthSteps)-0.5f;
    std::vector<float> times{-0.5f,end};
    const int intervals=std::max(1,static_cast<int>(
        std::ceil((end+0.5f)*40.0f)));
    times.reserve(static_cast<size_t>(intervals)+
        expressionLane(note,kind).size()*3u+extra.size()+2u);
    for(int i=1;i<intervals;++i)
        times.push_back(-0.5f+(end+0.5f)*
            static_cast<float>(i)/static_cast<float>(intervals));
    const auto& lane=expressionLane(note,kind);
    float preceding=-0.5f;
    for(const auto& point:lane){
        const float x=std::clamp(point.step,-0.5f,end);
        times.push_back(x);
        if(x>preceding+1.0e-5f)
            times.push_back(preceding+(x-preceding)*
                std::clamp(point.midpoint,0.05f,0.95f));
        preceding=x;
    }
    for(float v:extra)times.push_back(std::clamp(v,-0.5f,end));
    std::sort(times.begin(),times.end());
    times.erase(std::unique(times.begin(),times.end(),
        [](float a,float b){return std::fabs(a-b)<0.00001f;}),
        times.end());
    return times;
}

// One continuous brush pass has one additive strength, independent of how
// many motion events the Android device sent. Multiple completed passes stack.
inline std::vector<CurvePoint> brushExpressionCurve(
    const Note& note,int kind,const std::vector<float>& touched,
    int polarity,float amount=0.05f) {
    if((kind!=1&&kind!=2)||touched.empty()||polarity==0)
        return expressionLane(note,kind);
    const auto& original=expressionLane(note,kind);
    std::vector<float> extras;
    extras.reserve(touched.size()*5u);
    for(float t:touched)
        for(float dx:{-0.75f,-0.20f,0.0f,0.20f,0.75f})
            extras.push_back(t+dx);
    const auto times=expressionSampleTimes(note,kind,extras);
    std::vector<RollDrawSample> samples;
    samples.reserve(times.size());
    for(float t:times){
        float closest=1.0e10f;
        for(float touch:touched)
            closest=std::min(closest,std::fabs(t-touch));
        float weight=0.0f;
        if(closest<=0.20f)weight=1.0f;
        else if(closest<0.75f){
            const float u=(0.75f-closest)/0.55f;
            weight=u*u*(3.0f-2.0f*u);
        }
        const float level=expressionCurveAt(original,kind,t);
        samples.push_back({t,std::clamp(
            level+static_cast<float>(polarity)*amount*weight,0.0f,1.0f)});
    }
    return simplifyRollDrawCurve(
        std::move(samples),
        std::max(kMinNoteLengthSteps,note.lengthSteps)-0.5f,
        0.0015f,kind==1?1.0f:0.0f);
}

// Redraw replaces pitch only where a stroke actually exists. The short
// edge crossfade makes a partial redraw connect to the untouched pitch curve.
inline std::vector<CurvePoint> redrawPitchCurve(
    const Note& note,std::vector<RollDrawSample> stroke) {
    if(stroke.size()<2)return note.bend;
    const float end=std::max(kMinNoteLengthSteps,note.lengthSteps)-0.5f;
    for(auto& point:stroke)point.step=std::clamp(point.step,-0.5f,end);
    std::stable_sort(stroke.begin(),stroke.end(),
        [](const RollDrawSample& a,const RollDrawSample& b){
            return a.step<b.step;});
    std::vector<RollDrawSample> draw;
    for(const auto& p:stroke){
        if(!draw.empty()&&std::fabs(draw.back().step-p.step)<0.0001f)
            draw.back().value=p.value;
        else draw.push_back(p);
    }
    if(draw.size()<2||draw.back().step-draw.front().step<0.0001f)
        return note.bend;
    const float lo=draw.front().step,hi=draw.back().step;
    // Scale the transition to the stroke: sub-grid notes should not be
    // swallowed by a fixed 0.15-step crossfade.
    const float fade=std::min(0.15f,std::max(0.0001f,(hi-lo)*0.20f));
    std::vector<float> extras{lo,hi,
        lo+fade,hi-fade};
    for(const auto& p:draw)extras.push_back(p.step);
    const auto times=expressionSampleTimes(note,0,extras);
    std::vector<RollDrawSample> samples;
    samples.reserve(times.size());
    size_t cursor=0;
    for(float t:times){
        const float original=expressionCurveAt(note.bend,0,t);
        float value=original;
        if(t>=lo&&t<=hi){
            while(cursor+1<draw.size()&&draw[cursor+1].step<t)++cursor;
            const auto& a=draw[cursor];
            const auto& b=draw[std::min(cursor+1,draw.size()-1)];
            const float f=b.step>a.step?
                std::clamp((t-a.step)/(b.step-a.step),0.0f,1.0f):0.0f;
            const float drawn=a.value+(b.value-a.value)*f;
            const float enter=lo<=-0.5f+0.0001f?1.0f:
                std::clamp((t-lo)/fade,0.0f,1.0f);
            const float leave=hi>=end-0.0001f?1.0f:
                std::clamp((hi-t)/fade,0.0f,1.0f);
            const float w=std::min(
                enter*enter*(3.0f-2.0f*enter),
                leave*leave*(3.0f-2.0f*leave));
            value=original+(drawn-original)*w;
        }
        samples.push_back({t,value});
    }
    return simplifyRollDrawCurve(std::move(samples),end,0.006f,0.0f);
}

inline bool sameExpressionCurve(
    const std::vector<CurvePoint>& a,const std::vector<CurvePoint>& b){
    if(a.size()!=b.size())return false;
    for(size_t i=0;i<a.size();++i){
        const auto& x=a[i];const auto& y=b[i];
        if(std::fabs(x.step-y.step)>0.0001f||
           std::fabs(x.value-y.value)>0.0001f||
           x.free!=y.free||
           std::fabs(x.midpoint-y.midpoint)>0.0001f||
           std::fabs(x.handleValue-y.handleValue)>0.0001f||
           x.handleActive!=y.handleActive)return false;
    }
    return true;
}

} // namespace aiora
