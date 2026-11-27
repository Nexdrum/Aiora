#include "PhysicalInstrumentVoice.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
using namespace aiora::physical;

struct Energy { double whole{}, late{}, peak{}; };
Energy render(Blueprint bp, float e1, float mute, float laterMute, int midi=57) {
    constexpr int sr=48000;
    PhysicalInstrumentVoice v;v.prepare(sr);v.start(bp,midi,e1,mute);
    Energy result;
    for(int i=0;i<sr*2;++i){
        if(i==sr/2)v.setExpression(e1,laterMute);
        const float x=v.render();
        assert(std::isfinite(x) && std::fabs(x)<=3.0f);
        result.whole+=x*x;
        if(i>=sr/2)result.late+=x*x;
        result.peak=std::max(result.peak,static_cast<double>(std::fabs(x)));
    }
    return result;
}
int main(){
    Blueprint b;
    PhysicalInstrumentVoice v;v.prepare(48000);v.start(b,57,0.8f);
    const float hz=440*std::pow(2.0f,(57-69)/12.0f);
    assert(std::fabs(v.targetFrequency()-hz)<0.01f);
    assert(v.stringCells()>=8&&v.stringCells()<=kMaxStringCells);
    assert(v.waveSpeedMetersPerSecond()>0);
    assert(v.effectiveLengthMeters()>0);
    const auto open=render(b,0.8f,0,0);
    const auto muted=render(b,0.8f,0,1);
    const auto quiet=render(b,0.20f,0,0);
    const auto strong=render(b,0.95f,0,0);
    assert(open.whole>1e-9&&open.peak>1e-6);
    assert(muted.late<open.late*0.8); // local contact drains vibrating energy
    assert(strong.whole>quiet.whole*8.0); // attack intensity E1
    Blueprint alternate=b;
    alternate.bodyMaterial=Material::Bronze;
    alternate.bodyGeometry=Geometry::Bell;
    const auto bell=render(alternate,0.8f,0,0);
    assert(std::fabs(bell.whole-open.whole)>open.whole*0.015);
    alternate.excitation=Excitation::Hammer;
    const auto hammered=render(alternate,0.8f,0,0);
    assert(hammered.whole>1e-10);
    for(int midi=36;midi<=96;midi+=12){
       for(Material mat:{Material::Steel,Material::Nylon,Material::Bronze,Material::Rubber}){
           alternate.stringMaterial=mat;
           const auto result=render(alternate,1,0,1,midi);
           assert(std::isfinite(result.whole));
           assert(result.peak<3);
       }
    }
    // Regression: an undriven long metal resonance must decay, not grow.
    Blueprint metallic=b;
    metallic.bodyMaterial=Material::Bronze;
    metallic.bodyGeometry=Geometry::Bell;
    PhysicalInstrumentVoice longVoice;
    longVoice.prepare(48000);
    longVoice.start(metallic,57,0.85f);
    double firstSecond=0, lastSecond=0;
    for(int i=0;i<48000*10;++i){
        const float x=longVoice.render();
        assert(std::isfinite(x)&&std::fabs(x)<3.0f);
        if(i<48000)firstSecond+=x*x;
        if(i>=48000*9)lastSecond+=x*x;
    }
    assert(lastSecond<firstSecond*0.01);
    v.release();v.stop();assert(!v.active()&&v.render()==0);
    std::printf("Physical instrument prototype passed. Open %.8g late %.8g muted late %.8g bell %.8g\n",
               open.whole,open.late,muted.late,bell.whole);
}
