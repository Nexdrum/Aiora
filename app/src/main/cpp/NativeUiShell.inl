// Layout geometry shared by render and hit tests. NativeUi.cpp includes this
// inside namespace aiora; the page content retains its original geometry API.
NativeUi::Rect NativeUi::expressionShellRect() const noexcept {
    const float w=std::max(1,width_-safeLeft_-safeRight_);
    const float h=std::max(1,height_-safeTop_-safeBottom_);
    const float g=std::clamp(std::min(w,h)*0.012f,8.0f,16.0f);
    const float x=safeLeft_+g,y=safeTop_+g;
    if(landscapeLayout())return {x,y,std::clamp(h*.45f,240.f,480.f),(h-3*g)*.5f};
    return {x,y,(w-3*g)*.5f,std::clamp(h*.24f,180.f,410.f)};
}
NativeUi::Rect NativeUi::transportShellRect() const noexcept {
    const auto a=expressionShellRect();
    const float w=width_-safeLeft_-safeRight_,h=height_-safeTop_-safeBottom_;
    const float g=std::clamp(std::min(w,h)*.012f,8.f,16.f);
    return landscapeLayout()?Rect{a.x,a.y+a.h+g,a.w,a.h}:
        Rect{a.x+a.w+g,a.y,a.w,a.h};
}
NativeUi::Rect NativeUi::expressionPadRect() const noexcept {
    const auto r=expressionShellRect();
    const float side=std::max(1.f,std::min(r.w-24.f,r.h-80.f));
    return {r.x+(r.w-side)*.5f,r.y+36.f,side,side};
}
NativeUi::Rect NativeUi::headerPerformanceRect() const noexcept { return {}; }
NativeUi::Rect NativeUi::headerScopeRect() const noexcept {
    const auto r=transportShellRect();
    const float inset=std::max(9.f,r.w*.04f);
    return {r.x+inset,r.y+r.h*.29f,r.w-2*inset,r.h*.32f};
}
NativeUi::Rect NativeUi::headerControlRect(int i) const noexcept {
    if(i<0||i>2)return {};
    const auto r=transportShellRect();
    const float gap=std::clamp(r.w*.018f,5.f,10.f);
    const float side=std::min(r.h*.21f,(r.w-4*gap)/3.4f);
    const float mid=side*1.25f;
    const float x=r.x+(r.w-(side*2+mid+2*gap))*.5f;
    const float y=r.y+r.h*.70f;
    if(i==0)return {x,y+(mid-side)*.5f,side,side};
    if(i==1)return {x+side+gap,y,mid,mid};
    return {x+side+gap+mid+gap,y+(mid-side)*.5f,side,side};
}
NativeUi::Rect NativeUi::navRect(int i) const noexcept {
    if(i<0||i>=7)return {};
    const float w=width_-safeLeft_-safeRight_,h=height_-safeTop_-safeBottom_;
    const float g=std::clamp(std::min(w,h)*.012f,8.f,16.f);
    const auto a=expressionShellRect();
    if(landscapeLayout()){
        const float buttonH=(h-9*g)/7.f;
        return {a.x+a.w+g,safeTop_+g+i*(buttonH+g),
                std::clamp(h*.095f,65.f,112.f),buttonH};
    }
    const float buttonW=(w-9*g)/7.f;
    return {safeLeft_+g+i*(buttonW+g),a.y+a.h+g,
            buttonW,std::clamp(w*.088f,55.f,88.f)};
}
NativeUi::Rect NativeUi::contentRect() const noexcept {
    const float w=width_-safeLeft_-safeRight_,h=height_-safeTop_-safeBottom_;
    const float g=std::clamp(std::min(w,h)*.012f,8.f,16.f);
    const auto n=navRect(0);
    if(landscapeLayout()){
        const float x=n.x+n.w+g;
        return {x,safeTop_+g,std::max(1.f,width_-safeRight_-g-x),h-2*g};
    }
    const float y=n.y+n.h+g;
    return {safeLeft_+g,y,w-2*g,
            std::max(1.f,height_-safeBottom_-g-y)};
}
