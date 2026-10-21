#include "ProjectStorage.h"

#include "ProjectCodec.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

namespace aiora {
namespace {
void setError(std::string* error,const std::string& value){if(error)*error=value;}
}

bool saveDrumKitFile(const std::string& path,const Track& track,std::string* error){
    if(path.empty()){setError(error,"Empty drum kit path");return false;}
    if(!track.drums||track.pads.empty()||track.pads.size()>49){
        setError(error,"No valid drum kit to export");return false;
    }
    const std::string temp=path+".tmp";
    {
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        if(!out){setError(error,"Could not open drum kit temp file");return false;}
        const std::string json=serializeDrumKitJson(track);
        out.write(json.data(),static_cast<std::streamsize>(json.size()));
        out.flush();
        if(!out){out.close();std::remove(temp.c_str());
            setError(error,"Could not write drum kit");return false;}
    }
    if(std::rename(temp.c_str(),path.c_str())!=0){
        std::remove(temp.c_str());
        setError(error,"Could not replace drum kit export");return false;
    }
    return true;
}

bool loadDrumKitFile(const std::string& path,std::vector<DrumPad>& pads,std::string* error){
    if(path.empty()){setError(error,"Empty drum kit path");return false;}
    std::ifstream in(path,std::ios::binary);
    if(!in){setError(error,"Could not open drum kit file");return false;}
    const std::string json((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    if(json.empty()){setError(error,"Drum kit file is empty");return false;}
    return deserializeDrumKitJson(json,pads,error);
}

bool saveProjectFile(const std::string& path,const Project& project,std::string* error){
    if(path.empty()){setError(error,"Empty project path");return false;}
    const std::string temp=path+".tmp";
    {
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        if(!out){setError(error,"Could not open autosave temp file");return false;}
        const std::string json=serializeProjectJson(project);
        out.write(json.data(),static_cast<std::streamsize>(json.size()));
        out.flush();
        if(!out){out.close();std::remove(temp.c_str());setError(error,"Could not write autosave");return false;}
    }
    if(std::rename(temp.c_str(),path.c_str())!=0){
        std::remove(temp.c_str());
        setError(error,"Could not replace autosave");
        return false;
    }
    return true;
}

bool loadProjectFile(const std::string& path,Project& project,std::string* error){
    if(path.empty()){setError(error,"Empty project path");return false;}
    std::ifstream in(path,std::ios::binary);
    if(!in){setError(error,"No autosave");return false;}
    const std::string json((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    if(json.empty()){setError(error,"Autosave is empty");return false;}
    return deserializeProjectJson(json,project,error);
}

bool savePatchFile(const std::string& path,const Patch& patch,std::string* error){
    if(path.empty()){setError(error,"Empty patch path");return false;}
    const std::string temp=path+".tmp";
    {
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        if(!out){setError(error,"Could not open patch temp file");return false;}
        const std::string json=serializePatchJson(patch);
        out.write(json.data(),static_cast<std::streamsize>(json.size()));
        out.flush();
        if(!out){out.close();std::remove(temp.c_str());setError(error,"Could not write patch");return false;}
    }
    if(std::rename(temp.c_str(),path.c_str())!=0){
        std::remove(temp.c_str());
        setError(error,"Could not replace patch file");
        return false;
    }
    return true;
}

bool loadPatchFile(const std::string& path,Patch& patch,std::string* error){
    if(path.empty()){setError(error,"Empty patch path");return false;}
    std::ifstream in(path,std::ios::binary);
    if(!in){setError(error,"Could not open patch file");return false;}
    const std::string json((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
    if(json.empty()){setError(error,"Patch file is empty");return false;}
    return deserializePatchJson(json,patch,error);
}

} // namespace aiora
