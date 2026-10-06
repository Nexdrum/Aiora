#include "NativeOverlay.h"
#include "NativeGlyphMasks.h"

#include <android/bitmap.h>
#include <jni.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace aiora {
namespace {

constexpr char kVs[] = R"(#version 300 es
layout(location=0) in vec2 aPos;
layout(location=1) in vec4 aColor;
out vec4 vColor;
void main(){
    gl_Position=vec4(aPos,0.0,1.0);
    vColor=aColor;
})";

constexpr char kFs[] = R"(#version 300 es
precision mediump float;
in vec4 vColor;
out vec4 fragColor;
void main(){
    fragColor=vColor;
})";

constexpr char kTextVs[] = R"(#version 300 es
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUv;
layout(location=2) in vec4 aColor;
out vec2 vUv;
out vec4 vColor;
void main(){
    gl_Position=vec4(aPos,0.0,1.0);
    vUv=aUv;
    vColor=aColor;
})";

constexpr char kTextFs[] = R"(#version 300 es
precision mediump float;
in vec2 vUv;
in vec4 vColor;
uniform sampler2D uFont;
out vec4 fragColor;
void main(){
    float a=texture(uFont,vUv).a;
    fragColor=vec4(vColor.rgb,vColor.a*a);
})";

GLuint compile(GLenum type,const char* src){
    const GLuint shader=glCreateShader(type);
    glShaderSource(shader,1,&src,nullptr);
    glCompileShader(shader);
    GLint ok=GL_FALSE;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(ok!=GL_TRUE){glDeleteShader(shader);return 0;}
    return shader;
}

constexpr std::array<uint32_t,8> kExtraCodepoints{
    0x00D7u,0x00B7u,0x2013u,0x2014u,
    0x2192u,0x223Fu,0x25C0u,0x25B6u
};

int glyphIndexForCodepoint(uint32_t cp) noexcept {
    if(cp>=32u&&cp<=126u)return static_cast<int>(cp-32u);
    for(size_t i=0;i<kExtraCodepoints.size();++i){
        if(kExtraCodepoints[i]==cp)
            return 95+static_cast<int>(i);
    }
    return static_cast<int>('?'-32);
}

uint32_t nextUtf8(std::string_view s,size_t& pos) noexcept {
    if(pos>=s.size())return 0u;
    const unsigned char c0=static_cast<unsigned char>(s[pos++]);
    if(c0<0x80u)return c0;
    if((c0&0xE0u)==0xC0u&&pos<s.size()){
        const unsigned char c1=static_cast<unsigned char>(s[pos++]);
        return ((c0&0x1Fu)<<6)|(c1&0x3Fu);
    }
    if((c0&0xF0u)==0xE0u&&pos+1<s.size()){
        const unsigned char c1=static_cast<unsigned char>(s[pos++]);
        const unsigned char c2=static_cast<unsigned char>(s[pos++]);
        return ((c0&0x0Fu)<<12)|((c1&0x3Fu)<<6)|(c2&0x3Fu);
    }
    if((c0&0xF8u)==0xF0u&&pos+2<s.size()){
        const unsigned char c1=static_cast<unsigned char>(s[pos++]);
        const unsigned char c2=static_cast<unsigned char>(s[pos++]);
        const unsigned char c3=static_cast<unsigned char>(s[pos++]);
        return ((c0&0x07u)<<18)|((c1&0x3Fu)<<12)|
               ((c2&0x3Fu)<<6)|(c3&0x3Fu);
    }
    return static_cast<uint32_t>('?');
}

JNIEnv* attachEnv(ANativeActivity* activity,bool& attached){
    attached=false;
    if(!activity||!activity->vm)return nullptr;
    JNIEnv* env=nullptr;
    const jint status=activity->vm->GetEnv(
        reinterpret_cast<void**>(&env),JNI_VERSION_1_6);
    if(status==JNI_OK)return env;
    if(status!=JNI_EDETACHED)return nullptr;
    if(activity->vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)return nullptr;
    attached=true;
    return env;
}

void detachEnv(ANativeActivity* activity,bool attached){
    if(attached&&activity&&activity->vm)activity->vm->DetachCurrentThread();
}

jclass loadAppClass(JNIEnv* env,jobject activity,const char* name){
    if(!env||!activity)return nullptr;
    jclass activityClass=env->GetObjectClass(activity);
    if(!activityClass)return nullptr;
    jmethodID getLoader=env->GetMethodID(
        activityClass,"getClassLoader","()Ljava/lang/ClassLoader;");
    jobject loader=getLoader?env->CallObjectMethod(activity,getLoader):nullptr;
    jclass loaderClass=loader?env->GetObjectClass(loader):nullptr;
    jmethodID loadClass=loaderClass
        ?env->GetMethodID(
            loaderClass,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;")
        :nullptr;
    jstring className=env->NewStringUTF(name);
    auto cls=loadClass&&className
        ?static_cast<jclass>(env->CallObjectMethod(loader,loadClass,className))
        :nullptr;
    if(env->ExceptionCheck())env->ExceptionClear();
    if(className)env->DeleteLocalRef(className);
    if(loaderClass)env->DeleteLocalRef(loaderClass);
    if(loader)env->DeleteLocalRef(loader);
    env->DeleteLocalRef(activityClass);
    return cls;
}

std::array<uint8_t,7> glyph(char c){
    switch(c){
        case 'A':return {14,17,17,31,17,17,17};
        case 'B':return {30,17,17,30,17,17,30};
        case 'C':return {14,17,16,16,16,17,14};
        case 'D':return {30,17,17,17,17,17,30};
        case 'E':return {31,16,16,30,16,16,31};
        case 'F':return {31,16,16,30,16,16,16};
        case 'G':return {14,17,16,23,17,17,15};
        case 'H':return {17,17,17,31,17,17,17};
        case 'I':return {31,4,4,4,4,4,31};
        case 'J':return {7,2,2,2,18,18,12};
        case 'K':return {17,18,20,24,20,18,17};
        case 'L':return {16,16,16,16,16,16,31};
        case 'M':return {17,27,21,21,17,17,17};
        case 'N':return {17,25,21,19,17,17,17};
        case 'O':return {14,17,17,17,17,17,14};
        case 'P':return {30,17,17,30,16,16,16};
        case 'Q':return {14,17,17,17,21,18,13};
        case 'R':return {30,17,17,30,20,18,17};
        case 'S':return {15,16,16,14,1,1,30};
        case 'T':return {31,4,4,4,4,4,4};
        case 'U':return {17,17,17,17,17,17,14};
        case 'V':return {17,17,17,17,17,10,4};
        case 'W':return {17,17,17,21,21,21,10};
        case 'X':return {17,17,10,4,10,17,17};
        case 'Y':return {17,17,10,4,4,4,4};
        case 'Z':return {31,1,2,4,8,16,31};
        case '0':return {14,17,19,21,25,17,14};
        case '1':return {4,12,4,4,4,4,14};
        case '2':return {14,17,1,2,4,8,31};
        case '3':return {30,1,1,14,1,1,30};
        case '4':return {2,6,10,18,31,2,2};
        case '5':return {31,16,16,30,1,1,30};
        case '6':return {14,16,16,30,17,17,14};
        case '7':return {31,1,2,4,8,8,8};
        case '8':return {14,17,17,14,17,17,14};
        case '9':return {14,17,17,15,1,1,14};
        case '+':return {0,4,4,31,4,4,0};
        case '-':return {0,0,0,31,0,0,0};
        case '.':return {0,0,0,0,0,12,12};
        case '/':return {1,2,2,4,8,8,16};
        case ':':return {0,12,12,0,12,12,0};
        case '_':return {0,0,0,0,0,0,31};
        case '=':return {0,31,0,31,0,0,0};
        case '?':return {14,17,1,2,4,0,4};
        case ' ':return {0,0,0,0,0,0,0};
        default:return {14,17,1,2,4,0,4};
    }
}

} // namespace

NativeOverlay& NativeOverlay::instance(){static NativeOverlay o;return o;}

bool NativeOverlay::buildProgram(){
    const GLuint vs=compile(GL_VERTEX_SHADER,kVs);
    const GLuint fs=compile(GL_FRAGMENT_SHADER,kFs);
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
    program_=glCreateProgram();
    glAttachShader(program_,vs);glAttachShader(program_,fs);glLinkProgram(program_);
    glDeleteShader(vs);glDeleteShader(fs);
    GLint ok=GL_FALSE;glGetProgramiv(program_,GL_LINK_STATUS,&ok);
    if(ok!=GL_TRUE){glDeleteProgram(program_);program_=0;return false;}
    glGenBuffers(1,&vbo_);
    return vbo_!=0;
}

bool NativeOverlay::buildTextProgram(){
    const GLuint vs=compile(GL_VERTEX_SHADER,kTextVs);
    const GLuint fs=compile(GL_FRAGMENT_SHADER,kTextFs);
    if(!vs||!fs){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);return false;}
    textProgram_=glCreateProgram();
    glAttachShader(textProgram_,vs);
    glAttachShader(textProgram_,fs);
    glLinkProgram(textProgram_);
    glDeleteShader(vs);glDeleteShader(fs);
    GLint ok=GL_FALSE;
    glGetProgramiv(textProgram_,GL_LINK_STATUS,&ok);
    if(ok!=GL_TRUE){
        glDeleteProgram(textProgram_);
        textProgram_=0;
        return false;
    }
    glGenBuffers(1,&textVbo_);
    return textVbo_!=0;
}

bool NativeOverlay::buildFontAtlas(ANativeActivity* activity){
    bool attached=false;
    JNIEnv* env=attachEnv(activity,attached);
    if(!env||!activity||!activity->clazz)return false;

    jclass atlasClass=loadAppClass(
        env,activity->clazz,"com.nexdrum.aiora.FontAtlas");
    jmethodID build=atlasClass
        ?env->GetStaticMethodID(
            atlasClass,"build","(IIIII)Landroid/graphics/Bitmap;")
        :nullptr;

    constexpr int cellW=64;
    constexpr int cellH=72;
    constexpr int columns=16;
    constexpr int fontPx=52;
    constexpr int baseline=60;
    constexpr int xPad=6;

    jobject bitmap=build
        ?env->CallStaticObjectMethod(
            atlasClass,build,cellW,cellH,columns,fontPx,0)
        :nullptr;

    bool ok=false;
    AndroidBitmapInfo info{};
    void* raw=nullptr;
    if(bitmap &&
       AndroidBitmap_getInfo(env,bitmap,&info)==ANDROID_BITMAP_RESULT_SUCCESS &&
       info.format==ANDROID_BITMAP_FORMAT_RGBA_8888 &&
       AndroidBitmap_lockPixels(env,bitmap,&raw)==ANDROID_BITMAP_RESULT_SUCCESS){

        atlasWidth_=static_cast<int>(info.width);
        atlasHeight_=static_cast<int>(info.height);
        const auto* bytes=static_cast<const uint8_t*>(raw);

        for(int index=0;index<kGlyphCount;++index){
            const int cellX=(index%columns)*cellW;
            const int cellY=(index/columns)*cellH;
            int minX=cellW,maxX=-1,minY=cellH,maxY=-1;

            for(int yy=0;yy<cellH;++yy){
                const auto* row=bytes+
                    static_cast<size_t>(cellY+yy)*info.stride+
                    static_cast<size_t>(cellX)*4u;
                for(int xx=0;xx<cellW;++xx){
                    const uint8_t alpha=row[static_cast<size_t>(xx)*4u+3u];
                    if(alpha<=6u)continue;
                    minX=std::min(minX,xx);maxX=std::max(maxX,xx);
                    minY=std::min(minY,yy);maxY=std::max(maxY,yy);
                }
            }

            auto& g=glyphs_[static_cast<size_t>(index)];
            if(maxX>=minX&&maxY>=minY){
                g.u0=static_cast<float>(cellX+minX)/static_cast<float>(atlasWidth_);
                g.v0=static_cast<float>(cellY+minY)/static_cast<float>(atlasHeight_);
                g.u1=static_cast<float>(cellX+maxX+1)/static_cast<float>(atlasWidth_);
                g.v1=static_cast<float>(cellY+maxY+1)/static_cast<float>(atlasHeight_);
                g.xBearing=static_cast<float>(minX-xPad)/static_cast<float>(fontPx);
                g.yBearing=static_cast<float>(minY-baseline)/static_cast<float>(fontPx);
                g.width=static_cast<float>(maxX-minX+1)/static_cast<float>(fontPx);
                g.height=static_cast<float>(maxY-minY+1)/static_cast<float>(fontPx);
                g.advance=static_cast<float>(maxX-minX+5)/static_cast<float>(fontPx);
                g.valid=true;
            }else{
                g.advance=index==0?0.34f:0.50f;
                g.valid=false;
            }
        }

        glGenTextures(1,&fontTexture_);
        glBindTexture(GL_TEXTURE_2D,fontTexture_);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,4);
        glTexImage2D(
            GL_TEXTURE_2D,0,GL_RGBA,
            atlasWidth_,atlasHeight_,0,
            GL_RGBA,GL_UNSIGNED_BYTE,raw);
        glBindTexture(GL_TEXTURE_2D,0);
        ok=fontTexture_!=0;
        AndroidBitmap_unlockPixels(env,bitmap);
    }

    if(env->ExceptionCheck())env->ExceptionClear();
    if(bitmap)env->DeleteLocalRef(bitmap);
    if(atlasClass)env->DeleteLocalRef(atlasClass);
    detachEnv(activity,attached);
    return ok;
}

bool NativeOverlay::buildGlyphAtlas(){
    constexpr int cell=68;
    constexpr int pad=2;
    constexpr int columns=4;
    constexpr int rows=4;
    constexpr int source=64;
    glyphAtlasWidth_=cell*columns;
    glyphAtlasHeight_=cell*rows;

    std::vector<uint8_t> pixels(
        static_cast<size_t>(glyphAtlasWidth_)*
        static_cast<size_t>(glyphAtlasHeight_)*4u,0u);

    const auto copyMask=[&](int index,const uint64_t* mask){
        const int ox=(index%columns)*cell+pad;
        const int oy=(index/columns)*cell+pad;
        for(int y=0;y<source;++y){
            const uint64_t bits=mask[y];
            for(int x=0;x<source;++x){
                if((bits&(uint64_t{1}<<(63-x)))==0u)continue;
                const size_t p=(
                    static_cast<size_t>(oy+y)*static_cast<size_t>(glyphAtlasWidth_)+
                    static_cast<size_t>(ox+x))*4u;
                pixels[p+0u]=255u;
                pixels[p+1u]=255u;
                pixels[p+2u]=255u;
                pixels[p+3u]=255u;
            }
        }
    };

    for(int i=0;i<12;++i)
        copyMask(i,glyphmask::kPitch64[static_cast<size_t>(i)].data());
    copyMask(12,glyphmask::kLogo64.data());

    glGenTextures(1,&glyphTexture_);
    glBindTexture(GL_TEXTURE_2D,glyphTexture_);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    glTexImage2D(
        GL_TEXTURE_2D,0,GL_RGBA,
        glyphAtlasWidth_,glyphAtlasHeight_,0,
        GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glBindTexture(GL_TEXTURE_2D,0);
    return glyphTexture_!=0;
}

bool NativeOverlay::init(ANativeActivity* activity){
    if(!program_||!vbo_){
        if(!buildProgram())return false;
    }
    if(!textProgram_||!textVbo_)buildTextProgram();
    if(!fontTexture_&&textProgram_)buildFontAtlas(activity);
    if(!glyphTexture_&&textProgram_)buildGlyphAtlas();
    return program_&&vbo_;
}

void NativeOverlay::shutdown() noexcept {
    vertices_.clear();
    glyphVertices_.clear();
    textVertices_.clear();
    if(glyphTexture_){glDeleteTextures(1,&glyphTexture_);glyphTexture_=0;}
    if(fontTexture_){glDeleteTextures(1,&fontTexture_);fontTexture_=0;}
    if(textVbo_){glDeleteBuffers(1,&textVbo_);textVbo_=0;}
    if(textProgram_){glDeleteProgram(textProgram_);textProgram_=0;}
    if(vbo_){glDeleteBuffers(1,&vbo_);vbo_=0;}
    if(program_){glDeleteProgram(program_);program_=0;}
}

void NativeOverlay::begin(int width,int height){
    width_=std::max(1,width);
    height_=std::max(1,height);
    const float shortSide=static_cast<float>(std::min(width_,height_));
    fontScale_=std::clamp(shortSide/700.0f,1.08f,1.28f);
    vertices_.clear();
    glyphVertices_.clear();
    textVertices_.clear();
    vertices_.reserve(12000);
    glyphVertices_.reserve(6000);
    textVertices_.reserve(12000);
}

bool NativeOverlay::clipRect(Rect& r) const noexcept {
    if(r.w<=0.0f||r.h<=0.0f)return false;
    if(!clipEnabled_)return true;
    const float x0=std::max(r.x,clip_.x);
    const float y0=std::max(r.y,clip_.y);
    const float x1=std::min(r.x+r.w,clip_.x+clip_.w);
    const float y1=std::min(r.y+r.h,clip_.y+clip_.h);
    if(x1<=x0||y1<=y0)return false;
    r={x0,y0,x1-x0,y1-y0};
    return true;
}

void NativeOverlay::addRect(Rect r,Color c){
    if(!clipRect(r))return;
    const float x0=r.x/static_cast<float>(width_)*2.0f-1.0f;
    const float x1=(r.x+r.w)/static_cast<float>(width_)*2.0f-1.0f;
    const float y0=1.0f-r.y/static_cast<float>(height_)*2.0f;
    const float y1=1.0f-(r.y+r.h)/static_cast<float>(height_)*2.0f;
    const Vertex a{x0,y0,c.r,c.g,c.b,c.a};
    const Vertex b{x1,y0,c.r,c.g,c.b,c.a};
    const Vertex d{x0,y1,c.r,c.g,c.b,c.a};
    const Vertex e{x1,y1,c.r,c.g,c.b,c.a};
    vertices_.insert(vertices_.end(),{a,d,b,b,d,e});
}

void NativeOverlay::addMaskRun(float x,float y,float w,float h,Color c){
    addRect({x,y,w,h},c);
}

void NativeOverlay::addMaskRows(const uint32_t* rows,int rowCount,int columnCount,Rect rect,Color color){
    const float px=rect.w/static_cast<float>(columnCount);
    const float py=rect.h/static_cast<float>(rowCount);
    for(int row=0;row<rowCount;++row){
        const uint32_t bits=rows[row];
        int col=0;
        while(col<columnCount){
            const uint32_t mask=1u<<(columnCount-1-col);
            if((bits&mask)==0u){++col;continue;}
            const int start=col;
            while(col<columnCount&&(bits&(1u<<(columnCount-1-col)))!=0u)++col;
            addMaskRun(rect.x+start*px,rect.y+row*py,(col-start)*px,py,color);
        }
    }
}

void NativeOverlay::addMaskRows64(const uint64_t* rows,int rowCount,int columnCount,Rect rect,Color color){
    const float px=rect.w/static_cast<float>(columnCount);
    const float py=rect.h/static_cast<float>(rowCount);
    for(int row=0;row<rowCount;++row){
        const uint64_t bits=rows[row];
        int col=0;
        while(col<columnCount){
            const uint64_t mask=uint64_t{1}<<(columnCount-1-col);
            if((bits&mask)==0u){++col;continue;}
            const int start=col;
            while(col<columnCount&&
                  (bits&(uint64_t{1}<<(columnCount-1-col)))!=0u)++col;
            addMaskRun(
                rect.x+start*px,
                rect.y+row*py,
                (col-start)*px,
                py,
                color);
        }
    }
}

char NativeOverlay::normalizedChar(char c) noexcept {
    if(c>='a'&&c<='z')return static_cast<char>(c-'a'+'A');
    return c;
}

uint8_t NativeOverlay::fontRow(char c,int row) noexcept {
    if(row<0||row>=7)return 0;
    return glyph(normalizedChar(c))[static_cast<size_t>(row)];
}

float NativeOverlay::textWidth(std::string_view text,float scale) const noexcept {
    if(text.empty())return 0.0f;
    if(!fontTexture_){
        const float effective=scale*fontScale_;
        return static_cast<float>(text.size())*6.0f*effective-effective;
    }
    const float px=20.0f*scale*fontScale_;
    float w=0.0f;
    size_t pos=0;
    while(pos<text.size()){
        const uint32_t cp=nextUtf8(text,pos);
        const int index=glyphIndexForCodepoint(cp);
        w+=glyphs_[static_cast<size_t>(index)].advance*px;
    }
    return w;
}

void NativeOverlay::addTextQuad(
    float x,float y,float w,float h,
    float u0,float v0,float u1,float v1,
    Color c){
    if(w<=0.0f||h<=0.0f)return;
    Rect original{x,y,w,h};
    Rect clipped=original;
    if(!clipRect(clipped))return;
    if(clipped.x!=original.x||clipped.y!=original.y||
       clipped.w!=original.w||clipped.h!=original.h){
        const float tx0=(clipped.x-original.x)/original.w;
        const float ty0=(clipped.y-original.y)/original.h;
        const float tx1=(clipped.x+clipped.w-original.x)/original.w;
        const float ty1=(clipped.y+clipped.h-original.y)/original.h;
        const float ou0=u0,ov0=v0,ou1=u1,ov1=v1;
        u0=ou0+(ou1-ou0)*tx0;
        v0=ov0+(ov1-ov0)*ty0;
        u1=ou0+(ou1-ou0)*tx1;
        v1=ov0+(ov1-ov0)*ty1;
        x=clipped.x;y=clipped.y;w=clipped.w;h=clipped.h;
    }
    const float x0=x/static_cast<float>(width_)*2.0f-1.0f;
    const float x1=(x+w)/static_cast<float>(width_)*2.0f-1.0f;
    const float y0=1.0f-y/static_cast<float>(height_)*2.0f;
    const float y1=1.0f-(y+h)/static_cast<float>(height_)*2.0f;
    const TextVertex a{x0,y0,u0,v0,c.r,c.g,c.b,c.a};
    const TextVertex b{x1,y0,u1,v0,c.r,c.g,c.b,c.a};
    const TextVertex d{x0,y1,u0,v1,c.r,c.g,c.b,c.a};
    const TextVertex e{x1,y1,u1,v1,c.r,c.g,c.b,c.a};
    textVertices_.insert(textVertices_.end(),{a,d,b,b,d,e});
}

void NativeOverlay::addGlyphQuad(
    Rect r,float u0,float v0,float u1,float v1,Color c){
    if(r.w<=0.0f||r.h<=0.0f)return;
    const Rect original=r;
    if(!clipRect(r))return;
    if(r.x!=original.x||r.y!=original.y||r.w!=original.w||r.h!=original.h){
        const float tx0=(r.x-original.x)/original.w;
        const float ty0=(r.y-original.y)/original.h;
        const float tx1=(r.x+r.w-original.x)/original.w;
        const float ty1=(r.y+r.h-original.y)/original.h;
        const float ou0=u0,ov0=v0,ou1=u1,ov1=v1;
        u0=ou0+(ou1-ou0)*tx0;
        v0=ov0+(ov1-ov0)*ty0;
        u1=ou0+(ou1-ou0)*tx1;
        v1=ov0+(ov1-ov0)*ty1;
    }
    const float x0=r.x/static_cast<float>(width_)*2.0f-1.0f;
    const float x1=(r.x+r.w)/static_cast<float>(width_)*2.0f-1.0f;
    const float y0=1.0f-r.y/static_cast<float>(height_)*2.0f;
    const float y1=1.0f-(r.y+r.h)/static_cast<float>(height_)*2.0f;
    const TextVertex a{x0,y0,u0,v0,c.r,c.g,c.b,c.a};
    const TextVertex b{x1,y0,u1,v0,c.r,c.g,c.b,c.a};
    const TextVertex d{x0,y1,u0,v1,c.r,c.g,c.b,c.a};
    const TextVertex e{x1,y1,u1,v1,c.r,c.g,c.b,c.a};
    glyphVertices_.insert(glyphVertices_.end(),{a,d,b,b,d,e});
}

void NativeOverlay::addGlyphGradientQuad(
    Rect r,float u0,float v0,float u1,float v1,
    Color tl,Color tr,Color bl,Color br){
    if(r.w<=0.0f||r.h<=0.0f)return;
    const Rect original=r;
    if(!clipRect(r))return;

    float tx0=0.0f,ty0=0.0f,tx1=1.0f,ty1=1.0f;
    if(r.x!=original.x||r.y!=original.y||r.w!=original.w||r.h!=original.h){
        tx0=(r.x-original.x)/original.w;
        ty0=(r.y-original.y)/original.h;
        tx1=(r.x+r.w-original.x)/original.w;
        ty1=(r.y+r.h-original.y)/original.h;
        const float ou0=u0,ov0=v0,ou1=u1,ov1=v1;
        u0=ou0+(ou1-ou0)*tx0;
        v0=ov0+(ov1-ov0)*ty0;
        u1=ou0+(ou1-ou0)*tx1;
        v1=ov0+(ov1-ov0)*ty1;
    }

    const auto lerp=[](Color a,Color b,float t){
        return Color{
            a.r+(b.r-a.r)*t,
            a.g+(b.g-a.g)*t,
            a.b+(b.b-a.b)*t,
            a.a+(b.a-a.a)*t};
    };
    const auto sample=[&](float x,float y){
        return lerp(lerp(tl,tr,x),lerp(bl,br,x),y);
    };
    const Color ctl=sample(tx0,ty0);
    const Color ctr=sample(tx1,ty0);
    const Color cbl=sample(tx0,ty1);
    const Color cbr=sample(tx1,ty1);

    const float x0=r.x/static_cast<float>(width_)*2.0f-1.0f;
    const float x1=(r.x+r.w)/static_cast<float>(width_)*2.0f-1.0f;
    const float y0=1.0f-r.y/static_cast<float>(height_)*2.0f;
    const float y1=1.0f-(r.y+r.h)/static_cast<float>(height_)*2.0f;
    const TextVertex a{x0,y0,u0,v0,ctl.r,ctl.g,ctl.b,ctl.a};
    const TextVertex b{x1,y0,u1,v0,ctr.r,ctr.g,ctr.b,ctr.a};
    const TextVertex d{x0,y1,u0,v1,cbl.r,cbl.g,cbl.b,cbl.a};
    const TextVertex e{x1,y1,u1,v1,cbr.r,cbr.g,cbr.b,cbr.a};
    glyphVertices_.insert(glyphVertices_.end(),{a,d,b,b,d,e});
}

void NativeOverlay::addBitmapText(
    std::string_view text,float x,float y,float scale,Color color){
    scale*=fontScale_;
    float pen=x;
    for(char raw:text){
        const bool lower=raw>='a'&&raw<='z';
        const char ch=normalizedChar(raw);
        const float gs=lower?scale*0.82f:scale;
        const float xoff=(scale-gs)*0.45f;
        const float yoff=lower?scale*1.18f:0.0f;
        for(int row=0;row<7;++row){
            const uint8_t bits=fontRow(ch,row);
            int col=0;
            while(col<5){
                if((bits&(1u<<(4-col)))==0u){++col;continue;}
                const int start=col;
                while(col<5&&(bits&(1u<<(4-col)))!=0u)++col;
                const float rw=std::max(gs*0.58f,(col-start)*gs-gs*0.12f);
                addRect({
                    pen+xoff+start*gs,
                    y+yoff+row*gs+gs*0.10f,
                    rw,gs*0.78f
                },color);
            }
        }
        pen+=6.0f*scale;
    }
}

void NativeOverlay::addText(std::string_view text,float x,float y,float scale,Color color){
    if(scale<=0.0f)return;
    if(!fontTexture_){
        addBitmapText(text,x,y,scale,color);
        return;
    }

    const float px=20.0f*scale*fontScale_;
    const float baseline=y+px*0.82f;
    float pen=x;
    size_t pos=0;
    while(pos<text.size()){
        const uint32_t cp=nextUtf8(text,pos);
        const int index=glyphIndexForCodepoint(cp);
        const auto& g=glyphs_[static_cast<size_t>(index)];
        if(g.valid){
            addTextQuad(
                pen+g.xBearing*px,
                baseline+g.yBearing*px,
                g.width*px,
                g.height*px,
                g.u0,g.v0,g.u1,g.v1,
                color);
        }
        pen+=g.advance*px;
    }
}

void NativeOverlay::addTextCentered(std::string_view text,Rect r,float scale,Color color){
    const float w=textWidth(text,scale);
    const float px=fontTexture_?20.0f*scale*fontScale_:7.0f*scale*fontScale_;
    addText(text,r.x+(r.w-w)*0.5f,r.y+(r.h-px)*0.5f,scale,color);
}

void NativeOverlay::addPitchGlyph(int pitchClass,Rect rect,Color color){
    const int pc=((pitchClass%12)+12)%12;
    if(!glyphTexture_){
        addMaskRows64(glyphmask::kPitch64[static_cast<size_t>(pc)].data(),64,64,rect,color);
        return;
    }
    constexpr float cell=68.0f;
    constexpr float pad=2.0f;
    constexpr float source=64.0f;
    constexpr int columns=4;
    const float x=static_cast<float>(pc%columns)*cell+pad;
    const float y=static_cast<float>(pc/columns)*cell+pad;
    addGlyphQuad(
        rect,
        x/static_cast<float>(glyphAtlasWidth_),
        y/static_cast<float>(glyphAtlasHeight_),
        (x+source)/static_cast<float>(glyphAtlasWidth_),
        (y+source)/static_cast<float>(glyphAtlasHeight_),
        color);
}

void NativeOverlay::addLogo(Rect rect,Color color){
    if(!glyphTexture_){
        addMaskRows64(glyphmask::kLogo64.data(),64,64,rect,color);
        return;
    }
    constexpr float cell=68.0f;
    constexpr float pad=2.0f;
    constexpr float source=64.0f;
    constexpr int index=12;
    constexpr int columns=4;
    const float x=static_cast<float>(index%columns)*cell+pad;
    const float y=static_cast<float>(index/columns)*cell+pad;
    addGlyphQuad(
        rect,
        x/static_cast<float>(glyphAtlasWidth_),
        y/static_cast<float>(glyphAtlasHeight_),
        (x+source)/static_cast<float>(glyphAtlasWidth_),
        (y+source)/static_cast<float>(glyphAtlasHeight_),
        color);
}

void NativeOverlay::addSpectrumLogo(Rect rect){
    if(!glyphTexture_){
        addLogo(rect,{0.91f,0.925f,0.945f,1.0f});
        return;
    }
    constexpr float cell=68.0f;
    constexpr float pad=2.0f;
    constexpr float source=64.0f;
    constexpr int index=12;
    constexpr int columns=4;
    const float x=static_cast<float>(index%columns)*cell+pad;
    const float y=static_cast<float>(index/columns)*cell+pad;
    const float u0=x/static_cast<float>(glyphAtlasWidth_);
    const float v0=y/static_cast<float>(glyphAtlasHeight_);
    const float u1=(x+source)/static_cast<float>(glyphAtlasWidth_);
    const float v1=(y+source)/static_cast<float>(glyphAtlasHeight_);

    const auto expanded=[&](float amount){
        return Rect{
            rect.x-amount,rect.y-amount,
            rect.w+amount*2.0f,rect.h+amount*2.0f};
    };

    // Match the HTML logo's spectrum: green/cyan across the top,
    // red/orange at lower left, violet at lower right.
    const Color tl{0.67f,1.00f,0.00f,1.0f};
    const Color tr{0.00f,0.90f,1.00f,1.0f};
    const Color bl{1.00f,0.18f,0.00f,1.0f};
    const Color br{0.48f,0.00f,0.82f,1.0f};
    const auto fade=[](Color c,float a){c.a=a;return c;};

    addGlyphGradientQuad(
        expanded(7.0f),u0,v0,u1,v1,
        fade(tl,0.10f),fade(tr,0.10f),fade(bl,0.10f),fade(br,0.10f));
    addGlyphGradientQuad(
        expanded(3.5f),u0,v0,u1,v1,
        fade(tl,0.20f),fade(tr,0.20f),fade(bl,0.20f),fade(br,0.20f));
    addGlyphGradientQuad(rect,u0,v0,u1,v1,tl,tr,bl,br);
    addLogo(rect,{1.0f,1.0f,1.0f,0.16f});
}

void NativeOverlay::addLine(float x1,float y1,float x2,float y2,float thickness,Color c){
    if(clipEnabled_){
        float t0=0.0f,t1=1.0f;
        const float dx=x2-x1,dy=y2-y1;
        const float p[4]{-dx,dx,-dy,dy};
        const float q[4]{
            x1-clip_.x,
            clip_.x+clip_.w-x1,
            y1-clip_.y,
            clip_.y+clip_.h-y1};
        for(int i=0;i<4;++i){
            if(std::fabs(p[i])<1.0e-6f){
                if(q[i]<0.0f)return;
                continue;
            }
            const float t=q[i]/p[i];
            if(p[i]<0.0f)t0=std::max(t0,t);
            else t1=std::min(t1,t);
            if(t0>t1)return;
        }
        const float ox=x1,oy=y1;
        x1=ox+dx*t0;y1=oy+dy*t0;
        x2=ox+dx*t1;y2=oy+dy*t1;
    }
    const float dx=x2-x1,dy=y2-y1;
    const float len=std::sqrt(dx*dx+dy*dy);
    if(len<=0.001f||thickness<=0.0f)return;
    const float nx=-dy/len*thickness*0.5f;
    const float ny= dx/len*thickness*0.5f;
    auto vtx=[&](float x,float y){
        return Vertex{
            x/static_cast<float>(width_)*2.0f-1.0f,
            1.0f-y/static_cast<float>(height_)*2.0f,
            c.r,c.g,c.b,c.a
        };
    };
    const Vertex a=vtx(x1+nx,y1+ny);
    const Vertex b=vtx(x2+nx,y2+ny);
    const Vertex d=vtx(x1-nx,y1-ny);
    const Vertex e=vtx(x2-nx,y2-ny);
    vertices_.insert(vertices_.end(),{a,d,b,b,d,e});
}

void NativeOverlay::addCircle(float cx,float cy,float radius,float thickness,Color color){
    if(radius<=0.0f||thickness<=0.0f)return;
    constexpr int segments=24;
    float px=cx+radius,py=cy;
    for(int i=1;i<=segments;++i){
        const float a=static_cast<float>(i)*6.28318530718f/static_cast<float>(segments);
        const float x=cx+std::cos(a)*radius;
        const float y=cy+std::sin(a)*radius;
        addLine(px,py,x,y,thickness,color);
        px=x;py=y;
    }
}

void NativeOverlay::addChevron(Rect r,bool right,Color c){
    const float t=std::max(2.0f,std::min(r.w,r.h)*0.11f);
    const float cx=r.x+r.w*0.5f,cy=r.y+r.h*0.5f;
    const float dx=r.w*0.16f,dy=r.h*0.19f;
    if(right){
        addLine(cx-dx,cy-dy,cx+dx,cy,t,c);
        addLine(cx+dx,cy,cx-dx,cy+dy,t,c);
    }else{
        addLine(cx+dx,cy-dy,cx-dx,cy,t,c);
        addLine(cx-dx,cy,cx+dx,cy+dy,t,c);
    }
}

void NativeOverlay::addDownChevron(Rect r,Color c){
    const float t=std::max(1.5f,std::min(r.w,r.h)*0.08f);
    const float cx=r.x+r.w*0.5f,cy=r.y+r.h*0.52f;
    const float dx=r.w*0.16f,dy=r.h*0.12f;
    addLine(cx-dx,cy-dy,cx,cy+dy,t,c);
    addLine(cx,cy+dy,cx+dx,cy-dy,t,c);
}

void NativeOverlay::addNavIcon(int index,Rect r,Color c){
    const float s=std::min(r.w,r.h);
    const float cx=r.x+r.w*0.5f,cy=r.y+r.h*0.5f;
    const float t=std::max(2.0f,s*0.055f);

    switch(index){
        case 0:{ // tracks / mixer
            for(int i=0;i<3;++i){
                const float y=cy+s*(-0.22f+0.22f*i);
                addLine(cx-s*0.24f,y,cx+s*0.24f,y,t,c);
            }
            addCircle(cx+s*0.12f,cy-s*0.22f,s*0.055f,t,c);
            addCircle(cx-s*0.08f,cy,s*0.055f,t,c);
            addCircle(cx+s*0.05f,cy+s*0.22f,s*0.055f,t,c);
            break;
        }
        case 1:{ // drum
            const float top=cy-s*0.20f,bottom=cy+s*0.20f;
            addCircle(cx,top,s*0.20f,t,c);
            addLine(cx-s*0.20f,top,cx-s*0.17f,bottom,t,c);
            addLine(cx+s*0.20f,top,cx+s*0.17f,bottom,t,c);
            addLine(cx-s*0.17f,bottom,cx-s*0.08f,bottom+s*0.06f,t,c);
            addLine(cx+s*0.17f,bottom,cx+s*0.08f,bottom+s*0.06f,t,c);
            addLine(cx-s*0.08f,bottom+s*0.06f,cx+s*0.08f,bottom+s*0.06f,t,c);
            break;
        }
        case 2:{ // piano roll grid
            const float cell=s*0.12f,g=s*0.04f;
            const float total=cell*3.0f+g*2.0f;
            const float x0=cx-total*0.5f,y0=cy-total*0.5f;
            for(int y=0;y<3;++y)for(int x=0;x<3;++x)
                addRect({x0+x*(cell+g),y0+y*(cell+g),cell,cell},c);
            break;
        }
        case 3:{ // synth sliders
            const float xs[3]{cx-s*0.18f,cx,cx+s*0.18f};
            const float ys[3]{cy+s*0.11f,cy-s*0.13f,cy+s*0.02f};
            for(int i=0;i<3;++i){
                addLine(xs[i],cy-s*0.28f,xs[i],cy+s*0.28f,t,c);
                addCircle(xs[i],ys[i],s*0.06f,t,c);
            }
            break;
        }
        case 4:{ // fx wave
            float px=cx-s*0.27f,py=cy;
            constexpr int n=8;
            for(int i=1;i<n;++i){
                const float u=static_cast<float>(i)/static_cast<float>(n-1);
                const float x=cx-s*0.27f+u*s*0.54f;
                const float y=cy+std::sin(u*6.28318530718f)*s*0.13f;
                addLine(px,py,x,y,t,c);px=x;py=y;
            }
            addCircle(cx+s*0.22f,cy+s*0.12f,s*0.045f,t,c);
            break;
        }
        case 5:{ // keyboard
            const float x0=cx-s*0.27f,y0=cy-s*0.18f,w=s*0.54f,h=s*0.36f;
            addLine(x0,y0,x0+w,y0,t,c);
            addLine(x0,y0+h,x0+w,y0+h,t,c);
            addLine(x0,y0,x0,y0+h,t,c);
            addLine(x0+w,y0,x0+w,y0+h,t,c);
            for(int i=1;i<5;++i){
                const float x=x0+w*i/5.0f;
                addLine(x,y0,x,y0+h,t*0.7f,c);
            }
            for(float u:{0.20f,0.40f,0.70f,0.90f})
                addRect({x0+w*u-s*0.035f,y0,s*0.07f,h*0.52f},c);
            break;
        }
        default:break;
    }
}


void NativeOverlay::addDrumIcon(int index,Rect r,Color c){
    const float s=std::min(r.w,r.h);
    const float cx=r.x+r.w*0.5f,cy=r.y+r.h*0.5f;
    const float t=std::max(1.7f,s*0.055f);
    const float w=s*0.52f,h=s*0.42f;

    auto ellipse=[&](float ex,float ey,float rx,float ry){
        constexpr int seg=24;
        float px=ex+rx,py=ey;
        for(int k=1;k<=seg;++k){
            const float a=static_cast<float>(k)*6.28318530718f/static_cast<float>(seg);
            const float x=ex+std::cos(a)*rx;
            const float y=ey+std::sin(a)*ry;
            addLine(px,py,x,y,t,c);px=x;py=y;
        }
    };

    switch(index){
        case 0: // kick
            addCircle(cx,cy,s*0.28f,t,c);
            addCircle(cx,cy,s*0.07f,t,c);
            break;
        case 1: // snare
            ellipse(cx,cy-s*0.10f,w*0.50f,h*0.18f);
            addLine(cx-w*0.50f,cy-s*0.10f,cx-w*0.45f,cy+s*0.18f,t,c);
            addLine(cx+w*0.50f,cy-s*0.10f,cx+w*0.45f,cy+s*0.18f,t,c);
            ellipse(cx,cy+s*0.18f,w*0.45f,h*0.16f);
            addLine(cx-w*0.34f,cy-s*0.32f,cx-s*0.03f,cy-s*0.12f,t,c);
            addLine(cx+w*0.34f,cy-s*0.32f,cx+s*0.03f,cy-s*0.12f,t,c);
            break;
        case 2: // tom
            ellipse(cx,cy-s*0.16f,w*0.40f,h*0.16f);
            addLine(cx-w*0.40f,cy-s*0.16f,cx-w*0.34f,cy+s*0.18f,t,c);
            addLine(cx+w*0.40f,cy-s*0.16f,cx+w*0.34f,cy+s*0.18f,t,c);
            ellipse(cx,cy+s*0.18f,w*0.34f,h*0.13f);
            break;
        case 3: // floor tom
            ellipse(cx,cy-s*0.19f,w*0.40f,h*0.15f);
            addLine(cx-w*0.40f,cy-s*0.19f,cx-w*0.33f,cy+s*0.18f,t,c);
            addLine(cx+w*0.40f,cy-s*0.19f,cx+w*0.33f,cy+s*0.18f,t,c);
            ellipse(cx,cy+s*0.18f,w*0.33f,h*0.13f);
            addLine(cx-w*0.25f,cy+s*0.25f,cx-w*0.30f,cy+s*0.42f,t,c);
            addLine(cx+w*0.25f,cy+s*0.25f,cx+w*0.30f,cy+s*0.42f,t,c);
            break;
        case 4: // hi-hat
            addLine(cx-w*0.45f,cy-s*0.15f,cx+w*0.45f,cy-s*0.15f,t,c);
            addLine(cx-w*0.34f,cy+s*0.02f,cx+w*0.34f,cy+s*0.02f,t,c);
            addLine(cx,cy-s*0.38f,cx,cy+s*0.38f,t,c);
            break;
        case 5: // cymbal/crash
            {
                constexpr int seg=18;
                float px=cx-w*0.48f,py=cy;
                for(int k=1;k<=seg;++k){
                    const float u=static_cast<float>(k)/seg;
                    const float x=cx-w*0.48f+u*w*0.96f;
                    const float yy=cy-std::sin(u*3.14159265f)*s*0.25f;
                    addLine(px,py,x,yy,t,c);px=x;py=yy;
                }
                addCircle(cx,cy,s*0.035f,t,c);
                addLine(cx,cy+s*0.02f,cx,cy+s*0.38f,t,c);
                addLine(cx-s*0.13f,cy+s*0.38f,cx+s*0.13f,cy+s*0.38f,t,c);
            }
            break;
        case 6: // ride
            {
                constexpr int seg=18;
                float px=cx-w*0.50f,py=cy;
                for(int k=1;k<=seg;++k){
                    const float u=static_cast<float>(k)/seg;
                    const float x=cx-w*0.50f+u*w;
                    const float yy=cy-std::sin(u*3.14159265f)*s*0.30f;
                    addLine(px,py,x,yy,t,c);px=x;py=yy;
                }
                addCircle(cx,cy-s*0.02f,s*0.06f,t,c);
                addLine(cx,cy+s*0.04f,cx,cy+s*0.40f,t,c);
            }
            break;
        case 7: // bongo
            for(float side:{-0.18f,0.18f}){
                const float ex=cx+side*s;
                ellipse(ex,cy-s*0.14f,s*0.16f,s*0.07f);
                addLine(ex-s*0.16f,cy-s*0.14f,ex-s*0.13f,cy+s*0.20f,t,c);
                addLine(ex+s*0.16f,cy-s*0.14f,ex+s*0.13f,cy+s*0.20f,t,c);
            }
            break;
        case 8: // conga
            ellipse(cx,cy-s*0.27f,s*0.22f,s*0.08f);
            addLine(cx-s*0.22f,cy-s*0.27f,cx-s*0.15f,cy+s*0.28f,t,c);
            addLine(cx+s*0.22f,cy-s*0.27f,cx+s*0.15f,cy+s*0.28f,t,c);
            ellipse(cx,cy+s*0.28f,s*0.15f,s*0.06f);
            break;
        case 9: // clap
            for(float a:{0.0f,0.785398f,1.570796f,2.356194f}){
                const float dx=std::cos(a)*s*0.16f,dy=std::sin(a)*s*0.16f;
                addLine(cx+dx,cy+dy,cx+dx*2.0f,cy+dy*2.0f,t,c);
                addLine(cx-dx,cy-dy,cx-dx*2.0f,cy-dy*2.0f,t,c);
            }
            break;
        case 10: // shaker
            addCircle(cx-s*0.08f,cy-s*0.08f,s*0.20f,t,c);
            addLine(cx+s*0.06f,cy+s*0.06f,cx+s*0.32f,cy+s*0.32f,t,c);
            break;
        case 11: // cowbell
            addLine(cx-s*0.20f,cy-s*0.30f,cx+s*0.20f,cy-s*0.30f,t,c);
            addLine(cx-s*0.20f,cy-s*0.30f,cx-s*0.30f,cy+s*0.22f,t,c);
            addLine(cx+s*0.20f,cy-s*0.30f,cx+s*0.30f,cy+s*0.22f,t,c);
            addLine(cx-s*0.30f,cy+s*0.22f,cx+s*0.30f,cy+s*0.22f,t,c);
            addLine(cx,cy+s*0.22f,cx,cy+s*0.40f,t,c);
            break;
        default:
            addCircle(cx,cy,s*0.24f,t,c);
            break;
    }
}

void NativeOverlay::flush(){
    if(vertices_.empty()&&glyphVertices_.empty()&&textVertices_.empty())return;

    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    if(!vertices_.empty()&&program_&&vbo_){
        glUseProgram(program_);
        glBindBuffer(GL_ARRAY_BUFFER,vbo_);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(vertices_.size()*sizeof(Vertex)),
            vertices_.data(),
            GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),
            reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),
            reinterpret_cast<void*>(sizeof(float)*2));
        glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices_.size()));
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }

    const auto drawTextured=[&](
        const std::vector<TextVertex>& verts,GLuint texture){
        if(verts.empty()||!textProgram_||!textVbo_||!texture)return;
        glUseProgram(textProgram_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,texture);
        const GLint loc=glGetUniformLocation(textProgram_,"uFont");
        if(loc>=0)glUniform1i(loc,0);
        glBindBuffer(GL_ARRAY_BUFFER,textVbo_);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(verts.size()*sizeof(TextVertex)),
            verts.data(),GL_STREAM_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(
            0,2,GL_FLOAT,GL_FALSE,sizeof(TextVertex),
            reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(
            1,2,GL_FLOAT,GL_FALSE,sizeof(TextVertex),
            reinterpret_cast<void*>(sizeof(float)*2));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(
            2,4,GL_FLOAT,GL_FALSE,sizeof(TextVertex),
            reinterpret_cast<void*>(sizeof(float)*4));
        glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(verts.size()));
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(2);
        glBindTexture(GL_TEXTURE_2D,0);
    };

    drawTextured(glyphVertices_,glyphTexture_);
    drawTextured(textVertices_,fontTexture_);

    glBindBuffer(GL_ARRAY_BUFFER,0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

} // namespace aiora
