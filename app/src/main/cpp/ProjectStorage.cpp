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

} // namespace aiora
