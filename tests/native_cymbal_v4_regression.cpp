// Runtime validation of the experimental v4 physical strike-position layers.
#include "ProjectCodec.h"
#include "SpectrachordVoice.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace aiora;
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
Patch read(const char* path){
 std::ifstream f(path,std::ios::binary);
 require(bool(f),"Missing experimental cymbal patch");
 const std::string json((std::istreambuf_iterator<char>(f)),std::istreambuf_iterator<char>());
 Patch p;std::string err;
 if(!deserializePatchJson(json,p,&err))throw std::runtime_error(err);
 return p;
}
const OperatorModChannel* route(const Operator& op,int dest){
 for(const auto& m:op.modChannels)if(m.enabled&&static_cast<int>(m.destination)==dest)return &m;
 return nullptr;
}
double measure(const Patch& patch,int op,float v,float m){
 DspPatch p=toDspPatch(patch);
 for(int i=0;i<6;++i)p.ops[i].enabled=(i==op);
 SpectrachordVoice voice;
 voice.prepare(48000);
 voice.start(713,p,76,v,0,48000,VoiceAutomation{});
 voice.setLiveExpression(v,m);
 double energy=0.0;
 for(int frame=0;frame<8000;++frame){
  const float y=voice.render();
  require(std::isfinite(y),"Non-finite waveform");
  energy+=static_cast<double>(y)*y;
 }
 return energy;
}
void validate(const char* path,bool crash){
 const Patch patch=read(path);
 require(patch.ops[1].waveInteraction==WaveInteraction::PM,"OP2 not PM");
 require(patch.ops[3].waveInteraction==WaveInteraction::PM,"OP4 not PM");
 require(patch.ops[0].waveInteraction==WaveInteraction::CF,"OP1 not CF");
 require(patch.ops[2].waveInteraction==WaveInteraction::CF,"OP3 not CF");
 for(int idx:{1,3}){
  const auto* mod=route(patch.ops[idx],4);
  require(mod&&mod->response.source==1,"Resonance PM must follow M");
 }
 for(int idx:{0,2}){
  const auto* mod=route(patch.ops[idx],4);
  require(mod&&mod->response.source==0,"Impact tone must follow V");
 }
 const auto* washPos=route(patch.ops[5],0);
 const auto* washVel=route(patch.ops[5],2);
 require(washPos&&washPos->response.source==1,"Wash needs M gate");
 require(washVel&&washVel->response.source==0,"Wash needs V morph");
 const auto bellOff=measure(patch,2,.5f,.79f);
 const auto bellOn=measure(patch,2,.5f,.92f);
 require(bellOff==0.0,"Bell impact leaks below M80");
 require(bellOn>1.0e-12,"Bell impact missing inside M80-100");
 require(measure(patch,0,.5f,.90f)==0.0,"Plate impact leaks into bell");
 require(measure(patch,0,.5f,.50f)>1.0e-12,"Plate impact missing");
 const double bellPlate=measure(patch,3,.5f,.5f);
 const double bellCenter=measure(patch,3,.5f,.95f);
 require(bellPlate>1.0e-14,"Plate-region bell resonance absent");
 require(bellCenter>bellPlate,"Bell resonance should strengthen at bell");
 require(measure(patch,5,.5f,0)==0.0,"Wash audible at V50");
 require(measure(patch,5,crash?.5f:.75f,0)==0.0,"Wash starts below force threshold");
 require(measure(patch,5,1.0f,.25f)==0.0,"Wash outside M0-20");
 require(measure(patch,5,1.0f,0)>1.0e-12,"Hard edge wash absent");
 require(measure(patch,1,.5f,.2f)>1.0e-12,"Plate resonance absent");
 require(measure(patch,1,.5f,.7f)>1.0e-12,"Inner plate resonance absent");
 std::cout<<"PASS v4 "<<(crash?"crash":"ride")
          <<" bell_plate="<<bellPlate<<" bell_center="<<bellCenter<<"\n";
}
}
int main(){
 try{
  validate("auditions/nexdrum/Crash_Strike_Position_PM_v4.json",true);
  validate("auditions/nexdrum/Ride_Strike_Position_PM_v4.json",false);
 }catch(const std::exception& e){std::cerr<<"v4 cymbal failure: "<<e.what()<<"\n";return 1;}
 return 0;
}
