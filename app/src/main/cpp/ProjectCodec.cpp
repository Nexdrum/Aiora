#include "ProjectCodec.h"

#include "FactoryPresets.h"
#include "NexdrumKit.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace aiora {
namespace {

struct Json {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type{Type::Null};
    bool boolean{};
    double number{};
    std::string string;
    std::vector<Json> array;
    std::map<std::string,Json> object;

    [[nodiscard]] const Json* get(std::string_view key) const {
        if(type!=Type::Object)return nullptr;
        const auto it=object.find(std::string(key));
        return it==object.end()?nullptr:&it->second;
    }
};

class Parser {
public:
    explicit Parser(std::string_view text):text_(text){}

    Json parse(){
        skip();
        Json out=parseValue();
        skip();
        if(pos_!=text_.size())fail("trailing JSON data");
        return out;
    }

private:
    [[noreturn]] void fail(const char* what) const {
        throw std::runtime_error(std::string(what)+" at "+std::to_string(pos_));
    }

    void skip(){
        while(pos_<text_.size()&&std::isspace(static_cast<unsigned char>(text_[pos_])))++pos_;
    }

    char peek() const {return pos_<text_.size()?text_[pos_]:'\0';}
    char take(){if(pos_>=text_.size())fail("unexpected end");return text_[pos_++];}

    bool consume(std::string_view token){
        if(text_.substr(pos_,token.size())!=token)return false;
        pos_+=token.size();return true;
    }

    Json parseValue(){
        skip();
        const char c=peek();
        if(c=='{')return parseObject();
        if(c=='[')return parseArray();
        if(c=='"'){Json j;j.type=Json::Type::String;j.string=parseString();return j;}
        if(c=='-'||(c>='0'&&c<='9'))return parseNumber();
        if(consume("true")){Json j;j.type=Json::Type::Bool;j.boolean=true;return j;}
        if(consume("false")){Json j;j.type=Json::Type::Bool;j.boolean=false;return j;}
        if(consume("null"))return {};
        fail("invalid JSON value");
    }

    Json parseObject(){
        Json j;j.type=Json::Type::Object;
        if(take()!='{')fail("expected object");
        skip();
        if(peek()=='}'){++pos_;return j;}
        while(true){
            skip();if(peek()!='"')fail("expected object key");
            std::string key=parseString();
            skip();if(take()!=':')fail("expected colon");
            j.object.emplace(std::move(key),parseValue());
            skip();
            const char c=take();
            if(c=='}')break;
            if(c!=',')fail("expected comma");
        }
        return j;
    }

    Json parseArray(){
        Json j;j.type=Json::Type::Array;
        if(take()!='[')fail("expected array");
        skip();
        if(peek()==']'){++pos_;return j;}
        while(true){
            j.array.push_back(parseValue());
            skip();
            const char c=take();
            if(c==']')break;
            if(c!=',')fail("expected comma");
        }
        return j;
    }

    static void appendUtf8(std::string& out,unsigned cp){
        if(cp<=0x7Fu)out.push_back(static_cast<char>(cp));
        else if(cp<=0x7FFu){
            out.push_back(static_cast<char>(0xC0u|(cp>>6)));
            out.push_back(static_cast<char>(0x80u|(cp&0x3Fu)));
        }else{
            out.push_back(static_cast<char>(0xE0u|(cp>>12)));
            out.push_back(static_cast<char>(0x80u|((cp>>6)&0x3Fu)));
            out.push_back(static_cast<char>(0x80u|(cp&0x3Fu)));
        }
    }

    static int hex(char c){
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return 10+c-'a';
        if(c>='A'&&c<='F')return 10+c-'A';
        return -1;
    }

    std::string parseString(){
        if(take()!='"')fail("expected string");
        std::string out;
        while(true){
            if(pos_>=text_.size())fail("unterminated string");
            char c=take();
            if(c=='"')break;
            if(c!='\\'){out.push_back(c);continue;}
            c=take();
            switch(c){
                case '"':out.push_back('"');break;
                case '\\':out.push_back('\\');break;
                case '/':out.push_back('/');break;
                case 'b':out.push_back('\b');break;
                case 'f':out.push_back('\f');break;
                case 'n':out.push_back('\n');break;
                case 'r':out.push_back('\r');break;
                case 't':out.push_back('\t');break;
                case 'u':{
                    if(pos_+4>text_.size())fail("short unicode escape");
                    unsigned cp=0;
                    for(int i=0;i<4;++i){
                        const int v=hex(text_[pos_++]);
                        if(v<0)fail("bad unicode escape");
                        cp=(cp<<4)|static_cast<unsigned>(v);
                    }
                    appendUtf8(out,cp);
                    break;
                }
                default:fail("bad string escape");
            }
        }
        return out;
    }

    Json parseNumber(){
        const size_t begin=pos_;
        if(peek()=='-')++pos_;
        if(peek()=='0')++pos_;
        else{
            if(peek()<'1'||peek()>'9')fail("bad number");
            while(std::isdigit(static_cast<unsigned char>(peek())))++pos_;
        }
        if(peek()=='.'){
            ++pos_;
            if(!std::isdigit(static_cast<unsigned char>(peek())))fail("bad fraction");
            while(std::isdigit(static_cast<unsigned char>(peek())))++pos_;
        }
        if(peek()=='e'||peek()=='E'){
            ++pos_;if(peek()=='+'||peek()=='-')++pos_;
            if(!std::isdigit(static_cast<unsigned char>(peek())))fail("bad exponent");
            while(std::isdigit(static_cast<unsigned char>(peek())))++pos_;
        }
        const std::string token(text_.substr(begin,pos_-begin));
        Json j;j.type=Json::Type::Number;
        try{j.number=std::stod(token);}catch(...){fail("bad number");}
        if(!std::isfinite(j.number))fail("non-finite number");
        return j;
    }

    std::string_view text_;
    size_t pos_{0};
};

double num(const Json* j,double fallback){
    return j&&j->type==Json::Type::Number&&std::isfinite(j->number)?j->number:fallback;
}
bool boolean(const Json* j,bool fallback){
    return j&&j->type==Json::Type::Bool?j->boolean:fallback;
}
std::string str(const Json* j,std::string fallback={}){
    return j&&j->type==Json::Type::String?j->string:std::move(fallback);
}
float clampf(double v,float lo,float hi,float fallback){
    if(!std::isfinite(v))return fallback;
    return std::clamp(static_cast<float>(v),lo,hi);
}
int clampi(double v,int lo,int hi,int fallback){
    if(!std::isfinite(v))return fallback;
    return std::clamp(static_cast<int>(std::lround(v)),lo,hi);
}

class Writer {
public:
    Writer(){out_<<std::setprecision(9);}

    void raw(std::string_view s){out_<<s;}
    void number(double v){out_<<(std::isfinite(v)?v:0.0);}
    void boolean(bool v){out_<<(v?"true":"false");}
    void string(std::string_view s){
        out_<<'"';
        for(unsigned char c:s){
            switch(c){
                case '"':out_<<"\\\"";break;
                case '\\':out_<<"\\\\";break;
                case '\b':out_<<"\\b";break;
                case '\f':out_<<"\\f";break;
                case '\n':out_<<"\\n";break;
                case '\r':out_<<"\\r";break;
                case '\t':out_<<"\\t";break;
                default:
                    if(c<0x20u){
                        static constexpr char hex[]="0123456789ABCDEF";
                        out_<<"\\u00"<<hex[(c>>4)&15]<<hex[c&15];
                    }else out_<<static_cast<char>(c);
            }
        }
        out_<<'"';
    }
    [[nodiscard]] std::string finish(){return out_.str();}
private:
    std::ostringstream out_;
};

const char* waveName(Wave w){
    switch(w){
        case Wave::Sine:return "sine";
        case Wave::Saw:return "sawtooth";
        case Wave::Square:return "square";
        case Wave::Triangle:return "triangle";
        case Wave::Custom:return "custom";
        case Wave::Noise:return "noise";
        case Wave::Hiss:return "hiss";
    }
    return "sine";
}
Wave parseWave(std::string_view s){
    if(s=="sawtooth"||s=="saw")return Wave::Saw;
    if(s=="square")return Wave::Square;
    if(s=="triangle")return Wave::Triangle;
    if(s=="custom")return Wave::Custom;
    if(s=="noise")return Wave::Noise;
    if(s=="hiss")return Wave::Hiss;
    return Wave::Sine;
}
const char* lfoName(LfoTarget t){
    switch(t){
        case LfoTarget::None:return "none";
        case LfoTarget::Pitch:return "pitch";
        case LfoTarget::Filter:return "filter";
        case LfoTarget::Amp:return "amp";
    }
    return "none";
}
LfoTarget parseLfo(std::string_view s){
    if(s=="pitch")return LfoTarget::Pitch;
    if(s=="filter")return LfoTarget::Filter;
    if(s=="amp")return LfoTarget::Amp;
    return LfoTarget::None;
}
void writeFxRack(Writer& w,Fx fx){
    migrateLegacyFx(fx);
    w.raw("[");
    for(size_t i=0;i<fx.count;++i){
        if(i)w.raw(",");
        const auto& m=fx.chain[i];
        w.raw("{\"type\":");w.number(static_cast<int>(m.type));
        w.raw(",\"on\":");w.boolean(m.enabled);
        w.raw(",\"p\":[");
        for(size_t j=0;j<fxParamCount(m.type);++j){
            if(j)w.raw(",");
            w.number(m.params[j]);
        }
        w.raw("]}");
    }
    w.raw("]");
}
bool readFxRack(const Json* source,Fx& fx){
    if(!source||source->type!=Json::Type::Array)return false;
    fx.rackConfigured=true;fx.count=0;
    for(const auto& item:source->array){
        if(fx.count>=kMaxFxStages)break;
        if(item.type!=Json::Type::Object)continue;
        const int t=static_cast<int>(num(item.get("type"),-1));
        if(t<0||t>static_cast<int>(FxType::Cabeus))continue;
        auto module=makeFxModule(static_cast<FxType>(t));
        module.enabled=boolean(item.get("on"),true);
        if(const Json* a=item.get("p");a&&a->type==Json::Type::Array)
            for(size_t j=0;j<std::min(a->array.size(),fxParamCount(module.type));++j){
                const auto d=fxParamDesc(module.type,j);
                module.params[j]=clampf(num(&a->array[j],d.defaultValue),d.lo,d.hi,d.defaultValue);
            }
        fx.chain[fx.count++]=module;
    }
    return true;
}
template<size_t N>
void writeFloatArray(Writer& w,const std::array<float,N>& a){
    w.raw("[");for(size_t i=0;i<a.size();++i){if(i)w.raw(",");w.number(a[i]);}w.raw("]");
}
void writeOperatorCurve(Writer& w,const OperatorCurve& c){
    w.raw("{\"enabled\":");w.boolean(c.enabled);
    w.raw(",\"sustain\":");w.boolean(c.sustain);
    w.raw(",\"releaseIndex\":");w.number(c.releaseIndex);
    w.raw(",\"decayIndex\":");w.number(c.decayIndex);
    w.raw(",\"points\":[");
    for(size_t i=0;i<c.count;++i){
        if(i)w.raw(",");const auto& p=c.points[i];
        w.raw("{\"t\":");w.number(p.time);w.raw(",\"v\":");w.number(p.value);
        w.raw(",\"mid\":");w.number(p.midpoint);
        if(p.free)w.raw(",\"free\":true");
        if(p.handleActive){w.raw(",\"handle\":");w.number(p.handleValue);}
        w.raw("}");
    }
    w.raw("]}");
}
void writeFilterLane(Writer& w,const FilterLane& lane){
    const auto& c=lane.curve;
    w.raw("{\"enabled\":");w.boolean(c.enabled);
    w.raw(",\"sustain\":");w.boolean(c.sustain);
    w.raw(",\"releaseIndex\":");w.number(c.releaseIndex);
    w.raw(",\"decayIndex\":");w.number(c.decayIndex);
    w.raw(",\"points\":[");
    for(size_t i=0;i<c.count;++i){
        if(i)w.raw(",");
        const auto& p=c.points[i];
        w.raw("{\"t\":");w.number(p.time);
        w.raw(",\"v\":");w.number(p.value);
        w.raw(",\"mid\":");w.number(p.midpoint);
        if(p.handleActive){w.raw(",\"handle\":");w.number(p.handleValue);}
        w.raw(",\"shape\":");w.number(static_cast<int>(lane.shape[i]));
        w.raw(",\"transition\":");w.number(lane.transition[i]);
        w.raw("}");
    }
    w.raw("]}");
}
const char* expressionSourceName(uint8_t source){
    return source==2?"K":source==1?"M":"V";
}
uint8_t parseExpressionSource(const std::string& source){
    return source=="K"?2:source=="M"?1:0;
}
void writeOperatorResponse(Writer& w,const OperatorResponse& c);
void writeEvolvingFilter(Writer& w,const EvolvingFilter& filter){
    if(!filter.configured){w.raw("null");return;}
    w.raw("{\"lanes\":[");
    for(size_t i=0;i<filter.lanes.size();++i){
        if(i)w.raw(",");
        writeFilterLane(w,filter.lanes[i]);
    }
    w.raw("],\"keyTrack\":");w.boolean(filter.keyTrack);
    w.raw(",\"slopes\":[");
    for(int i=0;i<3;++i){
        if(i)w.raw(",");
        const auto& s=filter.slopes[i];
        w.raw("{\"enabled\":");w.boolean(s.enabled);
        w.raw(",\"response\":");writeOperatorResponse(w,s.profile);
        w.raw("}");
    }
    w.raw("],\"mods\":[");
    for(size_t j=0;j<filter.mods.size();++j){
        if(j)w.raw(",");
        const auto& m=filter.mods[j];
        w.raw("{\"enabled\":");w.boolean(m.enabled);
        w.raw(",\"dest\":");w.number(m.destination);
        w.raw(",\"source\":");w.string(expressionSourceName(m.response.source));
        w.raw(",\"points\":[");
        for(int i=0;i<m.response.count;++i){
            if(i)w.raw(",");
            const auto& p=m.response.points[i];
            w.raw("{\"t\":");w.number(p.time);
            w.raw(",\"v\":");w.number(p.value);
            w.raw(",\"mid\":");w.number(p.midpoint);
            if(p.handleActive){w.raw(",\"handle\":");w.number(p.handleValue);}
            w.raw(",\"shape\":");w.number(static_cast<int>(m.shape[i]));
            w.raw(",\"transition\":");w.number(m.transition[i]);
            w.raw("}");
        }
        w.raw("]}");
    }
    w.raw("]}");
}
void writeOperatorResponse(Writer& w,const OperatorResponse& c){
    w.raw("{\"enabled\":");w.boolean(c.enabled);
    w.raw(",\"source\":");w.string(expressionSourceName(c.source));
    w.raw(",\"points\":[");
    for(size_t i=0;i<c.count;++i){
        if(i)w.raw(",");const auto& p=c.points[i];
        w.raw("{\"t\":");w.number(p.time);w.raw(",\"v\":");w.number(p.value);
        w.raw(",\"mid\":");w.number(p.midpoint);
        if(p.handleActive){w.raw(",\"handle\":");w.number(p.handleValue);}
        w.raw("}");
    }
    w.raw("]}");
}
void writePatch(Writer& w,const Patch& p){
    w.raw("{\"name\":");w.string(p.name);
    w.raw(",\"ops\":[");
    for(size_t i=0;i<p.ops.size();++i){
        if(i)w.raw(",");const auto& o=p.ops[i];
        w.raw("{\"enabled\":");w.boolean(o.enabled);
        w.raw(",\"randomPhase\":");w.boolean(o.randomPhase);
        w.raw(",\"randomPhaseDegrees\":");w.number(o.randomPhaseDegrees);
        w.raw(",\"wave\":");w.string(waveName(o.wave));
        if(o.waveMorphConfigured){
            w.raw(",\"waveMorph\":");w.string(waveName(o.waveMorph));
        }
        w.raw(",\"ratio\":");w.number(o.ratio);
        w.raw(",\"semi\":");w.number(o.semitoneOffset);
        w.raw(",\"detune\":");w.number(o.detuneCents);
        w.raw(",\"level\":");w.number(o.level);
        if(o.volumeCurve.enabled){w.raw(",\"volumeCurve\":");writeOperatorCurve(w,o.volumeCurve);}
        if(o.pitchCurve.enabled){w.raw(",\"pitchCurve\":");writeOperatorCurve(w,o.pitchCurve);}
        if(o.volumeMorphCurve.enabled){w.raw(",\"volumeMorphCurve\":");writeOperatorCurve(w,o.volumeMorphCurve);}
        if(o.pitchMorphCurve.enabled){w.raw(",\"pitchMorphCurve\":");writeOperatorCurve(w,o.pitchMorphCurve);}
        if(o.modChannelsConfigured){
            w.raw(",\"operatorMods\":[");
            for(size_t m=0;m<o.modChannels.size();++m){
                if(m)w.raw(",");
                const auto& channel=o.modChannels[m];
                w.raw("{\"enabled\":");w.boolean(channel.enabled);
                w.raw(",\"destination\":");w.number(static_cast<int>(channel.destination));
                w.raw(",\"response\":");writeOperatorResponse(w,channel.response);
                w.raw("}");
            }
            w.raw("]");
        }
        w.raw(",\"harm\":");if(o.hasHarm)writeFloatArray(w,o.harm);else w.raw("null");
        if(o.hasHarm){w.raw(",\"harmPhase\":");writeFloatArray(w,o.harmPhase);}
        if(o.hasHarmMute){
            w.raw(",\"harmMute\":");writeFloatArray(w,o.harmMute);
            w.raw(",\"harmMutePhase\":");writeFloatArray(w,o.harmMutePhase);
        }
        w.raw("}");
    }
    w.raw("],\"matrix\":[");
    for(size_t r=0;r<6;++r){if(r)w.raw(",");w.raw("[");for(size_t c=0;c<6;++c){if(c)w.raw(",");w.number(p.matrix[r][c]);}w.raw("]");}
    w.raw("],\"filter\":{\"evolving\":");writeEvolvingFilter(w,p.filter.evolving);w.raw("}");
    w.raw(",\"peak\":{\"enabled\":");w.boolean(p.peak.enabled);
    w.raw(",\"followNote\":");w.boolean(p.peak.followNote);
    w.raw(",\"ratio\":");w.number(p.peak.ratio);
    w.raw(",\"frequency\":");w.number(p.peak.frequency);
    w.raw(",\"q\":");w.number(p.peak.q);
    w.raw(",\"gainDb\":");w.number(p.peak.gainDb);w.raw("}");
    if(p.masterAmpCurve.enabled){w.raw(",\"masterAmpCurve\":");writeOperatorCurve(w,p.masterAmpCurve);}
    w.raw(",\"keyMap\":{\"low\":");w.number(p.keyMapLow);
    w.raw(",\"high\":");w.number(p.keyMapHigh);w.raw("}");
    w.raw(",\"lfo\":{\"rate\":");w.number(p.lfo.rate);w.raw(",\"amount\":");w.number(p.lfo.amount);w.raw(",\"target\":");w.string(lfoName(p.lfo.target));w.raw(",\"attack\":");w.number(p.lfo.attack);w.raw(",\"velSens\":");w.number(p.lfo.velocitySensitivity);w.raw("}");
    w.raw(",\"fx\":{\"dist\":");w.number(p.fx.distortion);
    w.raw(",\"distIn\":");w.number(p.fx.distIn);w.raw(",\"distOut\":");w.number(p.fx.distOut);
    w.raw(",\"delay\":");w.number(p.fx.delay);w.raw(",\"delayTime\":");w.number(p.fx.delayTime);w.raw(",\"delayFb\":");w.number(p.fx.delayFeedback);
    w.raw(",\"reverb\":");w.number(p.fx.reverb);
    w.raw(",\"reverbDecay\":");w.number(p.fx.reverbDecay);
    w.raw(",\"reverbRoom\":");w.number(p.fx.reverbRoom);
    w.raw("}");
    w.raw(",\"fxChain\":");writeFxRack(w,p.fx);
    w.raw(",\"unison\":");w.number(p.unison);
    if(p.unisonCurve.enabled){w.raw(",\"unisonCurve\":");writeOperatorCurve(w,p.unisonCurve);}
    w.raw(",\"octave\":");w.number(p.octave);w.raw(",\"volume\":");w.number(p.volume);
    if(p.fundamentalMidi>=0){w.raw(",\"fundamental\":{\"pitch\":");w.number(p.fundamentalMidi);w.raw("}");}
    if(p.nexdrumLow>=0&&p.nexdrumHigh>=0){w.raw(",\"nexdrum\":{\"lo\":");w.number(p.nexdrumLow);w.raw(",\"hi\":");w.number(p.nexdrumHigh);w.raw("}");}
    w.raw("}");
}

template<size_t N>
bool readFloatArray(const Json* j,std::array<float,N>& dst){
    if(!j||j->type!=Json::Type::Array)return false;
    dst.fill(0.0f);
    for(size_t i=0;i<std::min<size_t>(N,j->array.size());++i)
        dst[i]=clampf(num(&j->array[i],0),0,1,0);
    return true;
}
OperatorPoint readOperatorPoint(const Json& j,float maxTime,float lo,float hi){
    OperatorPoint p;
    p.time=clampf(num(j.get("t"),0),0,maxTime,0);
    p.value=clampf(num(j.get("v"),0),lo,hi,0);
    p.midpoint=clampf(num(j.get("mid"),.5),.02f,.98f,.5f);
    p.free=boolean(j.get("free"),false);
    if(j.get("handle")){p.handleActive=true;p.handleValue=clampf(num(j.get("handle"),0),lo,hi,0);}
    return p;
}
void readOperatorCurve(const Json* j,OperatorCurve& out,float lo,float hi){
    if(!j||j->type!=Json::Type::Object)return;
    const auto* points=j->get("points");
    if(!points||points->type!=Json::Type::Array||points->array.size()<2)return;
    out={};
    for(const auto& raw:points->array){
        if(out.count>=kMaxOperatorPoints)break;
        if(raw.type==Json::Type::Object)
            out.points[out.count++]=readOperatorPoint(raw,120.0f,lo,hi);
    }
    if(out.count<2){out={};return;}
    std::stable_sort(out.points.begin(),out.points.begin()+out.count,
        [](const OperatorPoint&a,const OperatorPoint&b){return a.time<b.time;});
    for(size_t i=1;i<out.count;++i)
        out.points[i].time=std::max(out.points[i].time,out.points[i-1].time+.0001f);
    out.releaseIndex=static_cast<uint8_t>(clampi(num(j->get("releaseIndex"),out.count-2),0,out.count-2,out.count-2));
    out.decayIndex=static_cast<uint8_t>(clampi(num(j->get("decayIndex"),1),0,out.count-2,1));
    out.sustain=boolean(j->get("sustain"),true);
    out.enabled=boolean(j->get("enabled"),true);
}
void readOperatorResponse(const Json* j,OperatorResponse& out){
    if(!j||j->type!=Json::Type::Object)return;
    const auto* points=j->get("points");
    if(!points||points->type!=Json::Type::Array||points->array.size()<2)return;
    out={};
    for(const auto& raw:points->array){
        if(out.count>=kMaxOperatorPoints)break;
        if(raw.type==Json::Type::Object)
            out.points[out.count++]=readOperatorPoint(raw,1.0f,0,1);
    }
    if(out.count<2){out={};return;}
    std::stable_sort(out.points.begin(),out.points.begin()+out.count,
        [](const OperatorPoint&a,const OperatorPoint&b){return a.time<b.time;});
    out.source=parseExpressionSource(str(j->get("source"),"V"));
    out.enabled=boolean(j->get("enabled"),true);
}
void readEvolvingFilter(const Json* j,EvolvingFilter& e){
    if(!j||j->type!=Json::Type::Object)return;
    e=initialEvolvingFilter();
    e.keyTrack=boolean(j->get("keyTrack"),false);
    if(const auto* slopeArr=j->get("slopes");
       slopeArr&&slopeArr->type==Json::Type::Array){
        for(size_t i=0;i<std::min<size_t>(3,slopeArr->array.size());++i){
            const auto& raw=slopeArr->array[i];
            if(raw.type!=Json::Type::Object)continue;
            auto& s=e.slopes[i];
            s.enabled=boolean(raw.get("enabled"),false);
            readOperatorResponse(raw.get("response"),s.profile);
        }
    }
    auto shapeOf=[](const Json* p){
        return static_cast<FilterShape>(clampi(num(p?p->get("shape"):nullptr,1),0,2,1));
    };
    if(const auto* lanes=j->get("lanes");
       lanes&&lanes->type==Json::Type::Array){
        for(size_t k=0;k<std::min<size_t>(6,lanes->array.size());++k){
            const auto& raw=lanes->array[k];
            const auto* points=raw.get("points");
            if(!points||points->type!=Json::Type::Array||points->array.size()<2)continue;
            FilterLane lane=initialFilterLane(static_cast<int>(k));
            lane.curve.count=0;
            for(const auto& item:points->array){
                if(item.type!=Json::Type::Object||lane.curve.count>=kMaxOperatorPoints)continue;
                const size_t i=lane.curve.count++;
                lane.curve.points[i]=readOperatorPoint(item,120.0f,0,1);
                lane.shape[i]=shapeOf(&item);
                lane.transition[i]=clampf(num(item.get("transition"),.65f),0,.95f,.65f);
            }
            if(lane.curve.count<2)continue;
            // Invalid or unsorted files cannot create backward time segments.
            for(int i=1;i<lane.curve.count;++i)
                lane.curve.points[i].time=std::max(
                    lane.curve.points[i].time,lane.curve.points[i-1].time+.0001f);
            lane.curve.sustain=boolean(raw.get("sustain"),true);
            lane.curve.enabled=boolean(raw.get("enabled"),true);
            lane.curve.releaseIndex=static_cast<uint8_t>(clampi(num(raw.get("releaseIndex"),lane.curve.count-2),0,lane.curve.count-2,lane.curve.count-2));
            lane.curve.decayIndex=static_cast<uint8_t>(clampi(num(raw.get("decayIndex"),1),0,lane.curve.count-2,1));
            e.lanes[k]=lane;
        }
    }
    if(const auto* mods=j->get("mods");mods&&mods->type==Json::Type::Array){
        for(size_t k=0;k<std::min<size_t>(e.mods.size(),mods->array.size());++k){
            const auto& raw=mods->array[k];
            if(raw.type!=Json::Type::Object)continue;
            auto& m=e.mods[k];m.enabled=boolean(raw.get("enabled"),false);
            m.destination=static_cast<uint8_t>(clampi(num(raw.get("dest"),0),0,5,0));
            m.response.source=parseExpressionSource(str(raw.get("source"),"V"));
            if(const auto* points=raw.get("points");
               points&&points->type==Json::Type::Array&&points->array.size()>=2){
                m.response.count=0;
                for(const auto& item:points->array){
                    if(item.type!=Json::Type::Object||m.response.count>=kMaxOperatorPoints)continue;
                    const size_t i=m.response.count++;
                    m.response.points[i]=readOperatorPoint(item,1,0,1);
                    m.shape[i]=shapeOf(&item);
                    m.transition[i]=clampf(num(item.get("transition"),.65f),0,.95f,.65f);
                }
                for(int i=1;i<m.response.count;++i)
                    m.response.points[i].time=std::max(
                        m.response.points[i].time,m.response.points[i-1].time+.0001f);
            }
        }
    }
}
bool readPatch(const Json& src,Patch& p){
    if(src.type!=Json::Type::Object)return false;
    const Json* ops=src.get("ops");
    if(!ops||ops->type!=Json::Type::Array||ops->array.size()!=6)return false;
    p=makeFactoryPatch(FactoryPreset::SpectrachordInit);
    p.name=str(src.get("name"),"Imported");
    for(size_t i=0;i<6;++i){
        const auto& jo=ops->array[i];if(jo.type!=Json::Type::Object)continue;auto& o=p.ops[i];
        o.enabled=boolean(jo.get("enabled"),o.enabled);
        o.randomPhase=boolean(jo.get("randomPhase"),false);
        // 0.7.69 toggle-only patches migrate to a subtle +/-10 degree range.
        o.randomPhaseDegrees=clampf(num(jo.get("randomPhaseDegrees"),10.0f),
            0.0f,180.0f,10.0f);
        o.wave=parseWave(str(jo.get("wave"),"sine"));
        if(const Json* altWave=jo.get("waveMorph")){
            o.waveMorph=parseWave(str(altWave,"sine"));
            o.waveMorphConfigured=true;
        }else{
            o.waveMorph=o.wave;
            o.waveMorphConfigured=false;
        }
        o.ratio=clampf(num(jo.get("ratio"),o.ratio),.125f,16.0f,o.ratio);

        float semi=clampf(
            num(jo.get("semi"),o.semitoneOffset),
            -12.0f,12.0f,o.semitoneOffset);
        float fine=static_cast<float>(
            num(jo.get("detune"),o.detuneCents));

        // Legacy patches only had ±100-cent detune. Fold any excess beyond
        // the new ±50-cent fine range into whole semitone steps so their
        // audible pitch is preserved.
        while(fine>50.0f&&semi<12.0f){
            fine-=100.0f;
            semi+=1.0f;
        }
        while(fine<-50.0f&&semi>-12.0f){
            fine+=100.0f;
            semi-=1.0f;
        }
        o.semitoneOffset=std::round(
            std::clamp(semi,-12.0f,12.0f));
        o.detuneCents=std::round(
            std::clamp(fine,-50.0f,50.0f));
        o.level=clampf(num(jo.get("level"),o.level),0,1,o.level);
        readOperatorCurve(jo.get("volumeCurve"),o.volumeCurve,0,1);
        readOperatorCurve(jo.get("pitchCurve"),o.pitchCurve,-12,12);
        readOperatorCurve(jo.get("volumeMorphCurve"),o.volumeMorphCurve,0,1);
        readOperatorCurve(jo.get("pitchMorphCurve"),o.pitchMorphCurve,-12,12);
        if(const auto* mods=jo.get("operatorMods");
           mods&&mods->type==Json::Type::Array){
            o.modChannelsConfigured=true;
            for(auto& channel:o.modChannels)channel=initialOperatorModChannel();
            for(size_t m=0;m<std::min<size_t>(o.modChannels.size(),mods->array.size());++m){
                const auto& raw=mods->array[m];
                if(raw.type!=Json::Type::Object)continue;
                auto& channel=o.modChannels[m];
                channel.enabled=boolean(raw.get("enabled"),false);
                channel.destination=static_cast<OperatorModDestination>(
                    clampi(num(raw.get("destination"),0),0,4,0));
                readOperatorResponse(raw.get("response"),channel.response);
            }
        }
        o.hasHarm=readFloatArray(jo.get("harm"),o.harm);
        o.hasHarmMute=readFloatArray(jo.get("harmMute"),o.harmMute);
        // Historical 16-partial patches have no phase arrays: zero phase
        // preserves their original authored sine-series alignment.
        readFloatArray(jo.get("harmPhase"),o.harmPhase);
        readFloatArray(jo.get("harmMutePhase"),o.harmMutePhase);
    }
    if(const Json* matrix=src.get("matrix");matrix&&matrix->type==Json::Type::Array){
        for(size_t r=0;r<std::min<size_t>(6,matrix->array.size());++r)if(matrix->array[r].type==Json::Type::Array)
            for(size_t c=0;c<std::min<size_t>(6,matrix->array[r].array.size());++c)p.matrix[r][c]=clampf(num(&matrix->array[r].array[c],0),0,1,0);
    }
    if(const Json* f=src.get("filter");f&&f->type==Json::Type::Object){
        readEvolvingFilter(f->get("evolving"),p.filter.evolving);
    }
    // Old patches have no peak key and therefore keep it disabled.
    if(const Json* pk=src.get("peak");pk&&pk->type==Json::Type::Object){
        p.peak.enabled=boolean(pk->get("enabled"),false);
        p.peak.followNote=boolean(pk->get("followNote"),true);
        p.peak.ratio=clampf(num(pk->get("ratio"),4),.5f,8,4);
        p.peak.frequency=clampf(num(pk->get("frequency"),220),40,8000,220);
        p.peak.q=clampf(num(pk->get("q"),1),.3f,12,1);
        p.peak.gainDb=clampf(num(pk->get("gainDb"),0),-18,18,0);
    }
    if(const Json* km=src.get("keyMap");km&&km->type==Json::Type::Object){
        p.keyMapLow=clampi(num(km->get("low"),0),0,126,0);
        p.keyMapHigh=clampi(num(km->get("high"),127),p.keyMapLow+1,127,127);
    }
    if(const Json* l=src.get("lfo");l&&l->type==Json::Type::Object){
        p.lfo.rate=clampf(num(l->get("rate"),5),.1f,20,5);p.lfo.amount=clampf(num(l->get("amount"),0),0,2,0);p.lfo.target=parseLfo(str(l->get("target"),"none"));p.lfo.attack=clampf(num(l->get("attack"),0),0,2,0);p.lfo.velocitySensitivity=clampf(num(l->get("velSens"),0),0,1,0);
    }
    if(const Json* x=src.get("fx");x&&x->type==Json::Type::Object){
        p.fx.distortion=clampf(num(x->get("dist"),0),0,10,0);
        p.fx.distIn=clampf(num(x->get("distIn"),1),0,4,1);
        p.fx.distOut=clampf(num(x->get("distOut"),1),0,4,1);
        p.fx.delay=clampf(num(x->get("delay"),0),0,1,0);
        p.fx.delayTime=clampf(num(x->get("delayTime"),.32),.03f,1,.32f);
        p.fx.delayFeedback=clampf(num(x->get("delayFb"),.3),0,1,.3f);
        p.fx.reverb=clampf(num(x->get("reverb"),0),0,1,0);
        p.fx.reverbDecay=clampf(num(x->get("reverbDecay"),.8),.2f,12,.8f);
        p.fx.reverbRoom=clampf(num(x->get("reverbRoom"),1),.5f,2.5f,1);
    }
    readFxRack(src.get("fxChain"),p.fx);
    readOperatorCurve(src.get("masterAmpCurve"),p.masterAmpCurve,0,1);
    p.unison=clampf(num(src.get("unison"),0),0,1,0);
    readOperatorCurve(src.get("unisonCurve"),p.unisonCurve,0,1);
    p.octave=static_cast<float>(clampi(num(src.get("octave"),0),-2,2,0));p.volume=clampf(num(src.get("volume"),.8),0,1,.8f);
    if(const Json* f=src.get("fundamental");f&&f->type==Json::Type::Object)p.fundamentalMidi=clampi(num(f->get("pitch"),-1),0,127,-1);
    if(const Json* n=src.get("nexdrum");n&&n->type==Json::Type::Object){p.nexdrumLow=clampi(num(n->get("lo"),-1),0,127,-1);p.nexdrumHigh=clampi(num(n->get("hi"),-1),0,127,-1);}
    return true;
}

void writeCurve(Writer& w,const std::vector<CurvePoint>& points,bool bend){
    w.raw("[");
    for(size_t i=0;i<points.size();++i){if(i)w.raw(",");const auto&p=points[i];w.raw("{\"d\":");w.number(p.step);w.raw(bend?",\"o\":":",\"v\":");w.number(p.value);if(p.free)w.raw(",\"f\":true");if(std::fabs(p.midpoint-0.5f)>0.0001f){w.raw(",\"q\":");w.number(p.midpoint);}
        if(p.handleActive){w.raw(",\"h\":");w.number(p.handleValue);}
        w.raw("}");}
    w.raw("]");
}
void readCurve(const Json* j,std::vector<CurvePoint>& out,bool bend){
    out.clear();if(!j||j->type!=Json::Type::Array)return;
    for(size_t i=0;i<j->array.size();++i){const auto& p=j->array[i];if(p.type!=Json::Type::Object)continue;CurvePoint cp;cp.step=std::max(-0.5f,static_cast<float>(num(p.get("d"),0)));cp.value=bend?static_cast<float>(num(p.get("o"),0)):clampf(num(p.get("v"),1),0,1,1);cp.free=boolean(p.get("f"),false);cp.midpoint=clampf(num(p.get("q"),0.5),0.05,0.95,0.5);
        cp.handleActive=p.get("h")!=nullptr;
        cp.handleValue=bend?static_cast<float>(num(p.get("h"),0)):
            clampf(num(p.get("h"),0),0,1,0);
        out.push_back(cp);}
    std::stable_sort(out.begin(),out.end(),[](const CurvePoint&a,const CurvePoint&b){return a.step<b.step;});
    // Malformed and historical project files may contain stacked points.
    // Retain the last value at each time, matching the prior DSP overwrite.
    std::vector<CurvePoint> distinct;
    distinct.reserve(out.size());
    for(const auto& point:out){
        if(!distinct.empty() && std::fabs(distinct.back().step-point.step)<0.0001f)
            distinct.back()=point;
        else distinct.push_back(point);
    }
    out=std::move(distinct);
}

void setError(std::string* error,const std::string& value){if(error)*error=value;}

} // namespace

std::string serializePatchJson(const Patch& patch){
    Writer w;writePatch(w,patch);return w.finish();
}

bool deserializePatchJson(std::string_view json,Patch& patch,std::string* error){
    try{
        const Json root=Parser(json).parse();
        const Json* src=&root;
        if(root.type==Json::Type::Object)if(const Json* p=root.get("patch"))src=p;
        if(!readPatch(*src,patch)){setError(error,"Not a valid AIORA patch");return false;}
        return true;
    }catch(const std::exception& e){setError(error,e.what());return false;}
}


std::string serializeDrumKitJson(const Track& track){
    Writer w;
    w.raw("{\"type\":\"aiora-drum-kit\",\"version\":1,\"name\":");
    w.string(track.name);
    w.raw(",\"selectedPad\":");w.number(track.selectedPad);
    w.raw(",\"pads\":[");
    for(size_t i=0;i<track.pads.size();++i){
        if(i)w.raw(",");
        const auto& p=track.pads[i];
        w.raw("{\"id\":");w.string(p.id.empty()?"pad"+std::to_string(i+1):p.id);
        w.raw(",\"name\":");w.string(p.name);
        w.raw(",\"m\":");w.number(p.centerMidi);
        w.raw(",\"icon\":");w.string(p.icon);
        w.raw(",\"vol\":");w.number(p.volume);
        w.raw(",\"pan\":");w.number(p.pan);
        w.raw(",\"patch\":");writePatch(w,p.patch);
        w.raw(",\"range\":[");w.number(p.lowMidi);
        w.raw(",");w.number(p.highMidi);w.raw("]}");
    }
    w.raw("]}");
    return w.finish();
}

bool deserializeDrumKitJson(std::string_view json,
                            std::vector<DrumPad>& pads,
                            std::string* error){
    try{
        const Json root=Parser(json).parse();
        if(root.type!=Json::Type::Object||
           str(root.get("type"),"")!="aiora-drum-kit"||
           static_cast<int>(num(root.get("version"),-1))!=1){
            setError(error,"Not a supported AIORA drum kit");
            return false;
        }
        const Json* entries=root.get("pads");
        if(!entries||entries->type!=Json::Type::Array||
           entries->array.empty()||entries->array.size()>49){
            setError(error,"Kit must contain 1 to 49 pads");
            return false;
        }
        std::vector<DrumPad> decoded;
        decoded.reserve(entries->array.size());
        std::array<bool,49> usedNotes{};
        for(size_t i=0;i<entries->array.size();++i){
            const Json& item=entries->array[i];
            if(item.type!=Json::Type::Object){
                setError(error,"Kit contains an invalid pad");
                return false;
            }
            DrumPad p;
            const Json* range=item.get("range");
            const Json* patch=item.get("patch");
            if(!range||range->type!=Json::Type::Array||range->array.size()!=2||
               !patch||patch->type!=Json::Type::Object){
                setError(error,"Kit pad missing pitch range or synth patch");
                return false;
            }
            p.centerMidi=static_cast<int>(num(item.get("m"),-1));
            p.lowMidi=static_cast<int>(num(&range->array[0],-1));
            p.highMidi=static_cast<int>(num(&range->array[1],-1));
            if(p.lowMidi<38||p.highMidi>86||p.lowMidi>p.highMidi||
               p.centerMidi<p.lowMidi||p.centerMidi>p.highMidi){
                setError(error,"Kit pad pitch range is invalid (38-86)");
                return false;
            }
            for(int midi=p.lowMidi;midi<=p.highMidi;++midi){
                if(usedNotes[static_cast<size_t>(midi-38)]){
                    setError(error,"Kit has overlapping pad pitch ranges");
                    return false;
                }
                usedNotes[static_cast<size_t>(midi-38)]=true;
            }
            p.id=str(item.get("id"),"pad"+std::to_string(i+1));
            p.name=str(item.get("name"),"");
            p.icon=str(item.get("icon"),"kick");
            p.volume=clampf(num(item.get("vol"),1),0,1,1);
            p.pan=clampf(num(item.get("pan"),0),-1,1,0);
            for(const auto& other:decoded)if(other.id==p.id){
                setError(error,"Kit contains duplicate pad identifiers");
                return false;
            }
            if(!readPatch(*patch,p.patch)){
                setError(error,"Kit contains an invalid synth patch");
                return false;
            }
            p.patch.fundamentalMidi=p.centerMidi;
            decoded.push_back(std::move(p));
        }
        pads=std::move(decoded);
        return true;
    }catch(const std::exception& e){
        setError(error,e.what());
        return false;
    }
}

std::string serializeProjectJson(const Project& project){
    Writer w;
    w.raw("{\"name\":");w.string(project.name);w.raw(",\"bpm\":");w.number(project.bpm);w.raw(",\"beats\":");w.number(project.beats);w.raw(",\"div\":");w.number(project.divisions);w.raw(",\"dz\":");w.boolean(project.dozenal);
    w.raw(",\"mvol\":");w.number(project.masterVolume);
    w.raw(",\"mrev\":");w.number(project.masterReverb);
    w.raw(",\"mrevDecay\":");w.number(project.masterReverbDecay);
    w.raw(",\"mrevRoom\":");w.number(project.masterReverbRoom);
    w.raw(",\"masterFxChain\":");
    writeFxRack(w,project.masterFx.rackConfigured?project.masterFx:
        legacyMasterFx(project.masterReverb,project.masterReverbDecay,project.masterReverbRoom));
    w.raw(",\"tracks\":[");
    for(size_t ti=0;ti<project.tracks.size();++ti){
        if(ti)w.raw(",");const auto&t=project.tracks[ti];
        w.raw("{\"name\":");w.string(t.name);w.raw(",\"kind\":");w.string(t.drums?"drums":"melodic");w.raw(",\"patch\":");writePatch(w,t.patch);
        w.raw(",\"vol\":");w.number(t.volume);w.raw(",\"panV\":");w.number(t.pan);w.raw(",\"mute\":");w.boolean(t.mute);w.raw(",\"solo\":");w.boolean(t.solo);w.raw(",\"selPad\":");w.number(t.selectedPad);
        if(t.drumZoneLow>=0&&t.drumZoneHigh>=0){w.raw(",\"drumZone\":[");w.number(t.drumZoneLow);w.raw(",");w.number(t.drumZoneHigh);w.raw("]");}else w.raw(",\"drumZone\":null");
        w.raw(",\"pads\":[");
        for(size_t pi=0;pi<t.pads.size();++pi){
            if(pi)w.raw(",");const auto&p=t.pads[pi];w.raw("{\"id\":");w.string(p.id.empty()?("pad"+std::to_string(pi)):p.id);w.raw(",\"name\":");w.string(p.name);w.raw(",\"m\":");w.number(p.centerMidi);w.raw(",\"icon\":");w.string(p.icon);w.raw(",\"vol\":");w.number(p.volume);w.raw(",\"pan\":");w.number(p.pan);w.raw(",\"patch\":");writePatch(w,p.patch);w.raw(",\"range\":[");w.number(p.lowMidi);w.raw(",");w.number(p.highMidi);w.raw("]}");
        }
        w.raw("],\"notes\":[");
        for(size_t ni=0;ni<t.notes.size();++ni){
            if(ni)w.raw(",");const auto&n=t.notes[ni];w.raw("{\"s\":");w.number(n.startStep);w.raw(",\"m\":");w.number(n.midi);w.raw(",\"len\":");w.number(n.lengthSteps);
            if(!n.bend.empty()){w.raw(",\"bend\":");writeCurve(w,n.bend,true);}if(!n.velocity.empty()){w.raw(",\"vel\":");writeCurve(w,n.velocity,false);}if(!n.mod.empty()){w.raw(",\"mod\":");writeCurve(w,n.mod,false);}w.raw("}");
        }
        w.raw("]}");
    }
    w.raw("]}");return w.finish();
}

bool deserializeProjectJson(std::string_view json,Project& project,std::string* error){
    try{
        const Json root=Parser(json).parse();
        if(root.type!=Json::Type::Object){setError(error,"Project root is not an object");return false;}
        const Json* tracks=root.get("tracks");
        if(!tracks||tracks->type!=Json::Type::Array){setError(error,"Missing AIORA tracks array");return false;}
        Project out;
        out.name=str(root.get("name"),"Untitled Song");
        if(out.name.empty())out.name="Untitled Song";
        if(out.name.size()>80)out.name.resize(80);
        out.bpm=clampf(num(root.get("bpm"),112),12,288,112);out.beats=clampi(num(root.get("beats"),4),1,12,4);out.divisions=clampi(num(root.get("div"),4),1,12,4);out.dozenal=boolean(root.get("dz"),false);out.masterVolume=clampf(num(root.get("mvol"),.9),0,1,.9f);out.masterReverb=clampf(num(root.get("mrev"),0),0,1,0);
        out.masterReverbDecay=clampf(num(root.get("mrevDecay"),.8),.2f,12,.8f);
        out.masterReverbRoom=clampf(num(root.get("mrevRoom"),1),.5f,2.5f,1);
        readFxRack(root.get("masterFxChain"),out.masterFx);
        for(size_t ti=0;ti<tracks->array.size();++ti){
            const auto& jt=tracks->array[ti];if(jt.type!=Json::Type::Object)continue;
            Track t;t.name=str(jt.get("name"),"Track "+std::to_string(ti+1));t.drums=str(jt.get("kind"),"melodic")=="drums";t.volume=clampf(num(jt.get("vol"),.8),0,1,.8f);t.pan=clampf(num(jt.get("panV"),0),-1,1,0);t.mute=boolean(jt.get("mute"),false);t.solo=boolean(jt.get("solo"),false);
            t.patch=makeFactoryPatch(t.drums?FactoryPreset::Nexdrum:FactoryPreset::SpectrachordInit);
            if(const Json* p=jt.get("patch")){Patch decoded;if(readPatch(*p,decoded))t.patch=std::move(decoded);}
            if(const Json* z=jt.get("drumZone");z&&z->type==Json::Type::Array&&z->array.size()>=2){t.drumZoneLow=clampi(num(&z->array[0],-1),14,110,-1);t.drumZoneHigh=clampi(num(&z->array[1],-1),14,110,-1);if(t.drumZoneLow>t.drumZoneHigh)std::swap(t.drumZoneLow,t.drumZoneHigh);}
            if(t.drums)if(const Json* pads=jt.get("pads");pads&&pads->type==Json::Type::Array){
                for(size_t pi=0;pi<std::min<size_t>(49,pads->array.size());++pi){
                    const auto& jp=pads->array[pi];if(jp.type!=Json::Type::Object)continue;const int center=clampi(num(jp.get("m"),62),38,86,62);DrumPad p;p.centerMidi=center;p.lowMidi=center;p.highMidi=center;p.icon=str(jp.get("icon"),"kick");p.name=str(jp.get("name"),"");p.volume=clampf(num(jp.get("vol"),1),0,1,1);p.pan=clampf(num(jp.get("pan"),0),-1,1,0);p.id=str(jp.get("id"),"pad"+std::to_string(pi));
                    p.patch=makeFactoryPatch(FactoryPreset::SpectrachordInit);if(const Json* patch=jp.get("patch")){Patch decoded;if(readPatch(*patch,decoded))p.patch=std::move(decoded);}p.patch.fundamentalMidi=center;
                    if(const Json* range=jp.get("range");range&&range->type==Json::Type::Array&&range->array.size()>=2){p.lowMidi=clampi(num(&range->array[0],center),38,86,center);p.highMidi=clampi(num(&range->array[1],center),38,86,center);if(p.lowMidi>p.highMidi)std::swap(p.lowMidi,p.highMidi);p.centerMidi=std::clamp(center,p.lowMidi,p.highMidi);p.patch.fundamentalMidi=p.centerMidi;}
                    t.pads.push_back(std::move(p));
                }
            }
            if(t.drums&&t.pads.empty()){const auto& kit=nexdrumKit();t.pads.assign(kit.begin(),kit.end());for(size_t i=0;i<t.pads.size();++i)if(t.pads[i].id.empty())t.pads[i].id="pad"+std::to_string(i);}
            t.selectedPad=t.pads.empty()?0:std::clamp(clampi(num(jt.get("selPad"),0),0,static_cast<int>(t.pads.size())-1,0),0,static_cast<int>(t.pads.size())-1);
            if(const Json* notes=jt.get("notes");notes&&notes->type==Json::Type::Array){
                for(const auto& jn:notes->array){if(jn.type!=Json::Type::Object)continue;Note n;n.midi=clampi(num(jn.get("m"),62),2,122,62);n.startStep=std::max(0.0f,static_cast<float>(num(jn.get("s"),0)));n.lengthSteps=std::max(1.0f,static_cast<float>(num(jn.get("len"),1)));readCurve(jn.get("bend"),n.bend,true);readCurve(jn.get("vel"),n.velocity,false);readCurve(jn.get("mod"),n.mod,false);t.notes.push_back(std::move(n));}
            }
            out.tracks.push_back(std::move(t));
        }
        project=std::move(out);return true;
    }catch(const std::exception& e){setError(error,e.what());return false;}
}

} // namespace aiora
