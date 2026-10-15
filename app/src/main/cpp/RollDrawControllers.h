#pragma once

#include <algorithm>
#include <cmath>

namespace aiora {

// The grid's zoom changes controller *thickness*, never its gesture length.
// Default / close / overview correspond to 13 / 8 / 25 visible columns.
inline constexpr float rollDrawZoomThicknessScale(int zoomIndex) noexcept {
    return zoomIndex==1?1.20f:zoomIndex==2?0.80f:1.0f;
}

struct RollControllerRect {
    float x{},y{},w{},h{};
};

struct RollControllerPair {
    RollControllerRect velocity{},modulation{};
};

// Both controllers are measured from the same center point of the corner.
// They sit fully within the pitch header / time gutter respectively, with
// equal 0..100 travel distances even when the grid is zoomed.
inline RollControllerPair rollControllerPair(
    float viewportX,float viewportY,float viewportW,float viewportH,
    float gutter,float header) noexcept {
    constexpr float kGestureLength=320.0f;
    constexpr float kCornerGap=16.0f;
    constexpr float kEdgeInset=5.0f;
    constexpr float kViewportInset=8.0f;

    const float centerX=viewportX+gutter*0.5f;
    const float centerY=viewportY+header*0.5f;
    const float radius=std::max(gutter,header)*0.5f+kCornerGap;
    const float vStart=centerX+radius;
    const float mStart=centerY+radius;

    // Extremely small viewports constrain BOTH gesture lengths identically.
    const float travel=std::max(0.0f,std::min({
        kGestureLength,
        viewportX+viewportW-vStart-kViewportInset,
        viewportY+viewportH-mStart-kViewportInset
    }));
    const float vThickness=std::max(0.0f,header-2.0f*kEdgeInset);
    const float mThickness=std::max(0.0f,gutter-2.0f*kEdgeInset);
    return {
        {vStart,centerY-vThickness*.5f,travel,vThickness},
        {centerX-mThickness*.5f,mStart,mThickness,travel}
    };
}

} // namespace aiora
