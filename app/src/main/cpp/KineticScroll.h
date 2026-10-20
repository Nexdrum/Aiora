#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aiora {

// Reusable, allocation-free Android-style kinetic scroll for one axis.
// Values are screen pixels; positive scroll means content moves up/left.
// The caller owns limits, hit-testing, and whether a drag is a scroll.
class KineticScroll {
public:
    void stop() noexcept { active_=false;tracking_=false;velocity_=0.0f; }
    void begin(float finger,int64_t ms) noexcept {
        stop();
        tracking_=true;
        lastFinger_=finger;
        lastMs_=ms;
        totalMovement_=0.0f;
        samples_=0;
    }
    float move(float finger,int64_t ms) noexcept {
        if(!tracking_)return 0.0f;
        const float delta=lastFinger_-finger;
        const int64_t elapsed=ms-lastMs_;
        lastFinger_=finger;
        if(elapsed>0){
            const float seconds=std::clamp(elapsed*.001f,.001f,.075f);
            const float instantaneous=std::clamp(delta/seconds,-14000.0f,14000.0f);
            // Smooth velocity against the uneven cadence of touch packets.
            velocity_=samples_==0?instantaneous:
                velocity_*.46f+instantaneous*.54f;
            ++samples_;
            lastMs_=ms;
        }
        totalMovement_+=std::fabs(delta);
        return delta;
    }
    void end(int64_t ms,bool moved=true) noexcept {
        if(!tracking_)return;
        tracking_=false;
        // A stopped finger or tiny tap must never coast.
        if(!moved||samples_==0||totalMovement_<9.0f||
           ms-lastMs_>100||std::fabs(velocity_)<180.0f){
            active_=false;velocity_=0.0f;return;
        }
        active_=true;
        velocity_=std::clamp(velocity_,-12000.0f,12000.0f);
    }
    [[nodiscard]] bool active()const noexcept{return active_;}
    [[nodiscard]] bool tracking()const noexcept{return tracking_;}
    [[nodiscard]] float speed()const noexcept{return velocity_;}
    float advance(float dt) noexcept {
        if(!active_)return 0.0f;
        // Exponential drag is frame-rate independent. Closed-form integral
        // gives the same travel at 30, 60 or 120 FPS.
        constexpr float friction=2.85f;
        const float seconds=std::clamp(dt,0.0f,.075f);
        if(seconds<=0.0f)return 0.0f;
        const float factor=std::exp(-friction*seconds);
        const float displacement=velocity_*(1.0f-factor)/friction;
        velocity_*=factor;
        if(std::fabs(velocity_)<15.0f){velocity_=0.0f;active_=false;}
        return displacement;
    }
private:
    bool tracking_{false},active_{false};
    float lastFinger_{0.0f},velocity_{0.0f},totalMovement_{0.0f};
    int64_t lastMs_{0};
    int samples_{0};
};

} // namespace aiora
