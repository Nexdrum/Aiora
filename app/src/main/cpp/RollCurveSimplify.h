#pragma once

#include "NativeUi.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace aiora {

// Finish-time only, never on the real-time audio thread. Simplify a hand-drawn
// performance against the SAME segment evaluator used by the piano roll and
// the voice. No arbitrary point cap or time-based subsampling is imposed.
//
// The fitted curve must remain within tolerance of the recorded polyline,
// both at its vertices and between them. A monotone cubic X-handle may replace
// many samples; sharp peaks, reversals and other deviations force new points.
inline std::vector<CurvePoint> simplifyRollDrawCurve(
    std::vector<RollDrawSample> samples,float end,
    float tolerance,float defaultValue){
    std::vector<CurvePoint> result;
    if(samples.empty())return result;

    end=std::max(-0.5f,end);
    tolerance=std::max(0.00001f,tolerance);
    for(auto& p:samples)p.step=std::clamp(p.step,-0.5f,end);
    std::stable_sort(samples.begin(),samples.end(),
        [](const RollDrawSample&a,const RollDrawSample&b){
            return a.step<b.step;
        });
    std::vector<RollDrawSample> points;
    points.reserve(samples.size()+2);
    for(const auto& p:samples){
        if(!points.empty()&&
           std::fabs(points.back().step-p.step)<0.0001f)
            points.back().value=p.value;
        else points.push_back(p);
    }
    if(points.empty())return result;
    if(points.front().step>-0.5f)
        points.insert(points.begin(),{-0.5f,defaultValue});
    if(points.back().step<end-0.0001f)
        points.push_back({end,points.back().value});
    if(points.size()<2)return result;

    // An unchanged default automation curve needs no points in the song.
    bool changed=false;
    for(const auto& p:points)
        if(std::fabs(p.value-defaultValue)>0.0005f){
            changed=true;break;
        }
    if(!changed)return result;

    struct SegmentFit {
        float error{std::numeric_limits<float>::infinity()};
        float q{0.5f},handle{0.0f};
        size_t worst{0};
        bool shaped{false};
    };
    const auto measure=[&](size_t from,size_t to,
                           float q,float handle,bool shaped){
        SegmentFit fit;
        fit.q=q;fit.handle=handle;fit.shaped=shaped;
        fit.error=0.0f;
        fit.worst=from+(to-from)/2;
        const float start=points[from].step;
        const float duration=points[to].step-start;
        const float a=points[from].value,b=points[to].value;
        if(duration<=1.0e-7f)return fit;
        for(size_t i=from+1;i<=to;++i){
            const auto model=[&](float x){
                return curveSegment((x-start)/duration,
                    a,b,q,shaped,handle);
            };
            const float deviation=std::fabs(
                model(points[i].step)-points[i].value);
            if(deviation>fit.error){
                fit.error=deviation;
                fit.worst=i<to?i:to-1;
            }
            // A shaped cubic may depart from a sparsely sampled drawing
            // between vertices. Compare it with the original straight
            // stroke at quarter intervals as well as at the samples.
            if(shaped){
                const float x0=points[i-1].step,x1=points[i].step;
                for(const float t:{0.25f,0.50f,0.75f}){
                    const float x=x0+(x1-x0)*t;
                    const float source=points[i-1].value+
                        (points[i].value-points[i-1].value)*t;
                    const float between=std::fabs(model(x)-source);
                    if(between>fit.error){
                        fit.error=between;
                        fit.worst=i<to?i:i-1;
                    }
                }
            }
        }
        return fit;
    };

    const auto fitSpan=[&](size_t from,size_t to){
        SegmentFit best=measure(from,to,0.5f,0.0f,false);
        if(to<=from+1||best.error<=tolerance)return best;
        const float a=points[from].value,b=points[to].value;
        if(std::fabs(b-a)<1.0e-5f)return best;
        const float begin=points[from].step;
        const float width=points[to].step-begin;
        // All candidates are evaluated with the actual C1 X-handle
        // interpolator. The optimal shape remains bounded by its endpoints.
        constexpr std::array<float,7> positions{
            0.18f,0.28f,0.39f,0.50f,0.61f,0.72f,0.82f};
        for(const float q:positions){
            const float x=begin+width*q;
            auto it=std::lower_bound(points.begin()+from+1,
                points.begin()+to+1,x,
                [](const RollDrawSample& p,float x){return p.step<x;});
            const size_t hi=static_cast<size_t>(it-points.begin());
            const size_t lo=hi-1;
            const float segment=points[hi].step-points[lo].step;
            const float amount=segment>1.0e-7f?
                std::clamp((x-points[lo].step)/segment,0.0f,1.0f):1.0f;
            const float observed=points[lo].value+
                (points[hi].value-points[lo].value)*amount;
            const float handle=std::clamp(observed,std::min(a,b),std::max(a,b));
            const SegmentFit candidate=measure(from,to,q,handle,true);
            if(candidate.error<best.error-1.0e-6f)best=candidate;
        }
        return best;
    };

    struct Range {size_t begin,end;};
    std::vector<Range> pending{{0,points.size()-1}};
    std::vector<bool> keep(points.size(),false);
    std::vector<SegmentFit> incoming(points.size());
    keep.front()=true;
    keep.back()=true;
    while(!pending.empty()){
        const auto span=pending.back();
        pending.pop_back();
        const auto fit=fitSpan(span.begin,span.end);
        if(fit.error<=tolerance||span.end<=span.begin+1){
            incoming[span.end]=fit;
            continue;
        }
        const size_t split=std::clamp(fit.worst,
            span.begin+1,span.end-1);
        keep[split]=true;
        pending.push_back({split,span.end});
        pending.push_back({span.begin,split});
    }
    for(size_t i=0;i<points.size();++i){
        if(!keep[i])continue;
        const auto& fit=incoming[i];
        CurvePoint p{points[i].step,points[i].value,true,fit.q};
        if(i>0&&fit.shaped){
            p.handleValue=fit.handle;
            p.handleActive=true;
        }
        result.push_back(p);
    }
    return result;
}

} // namespace aiora
