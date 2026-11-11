#pragma once
#include <map>
#include <string>
#include <stdexcept>
namespace config {
inline std::map<std::string,std::string> values;inline unsigned writes{},flushes{};inline bool auto_saving{true};
inline std::string throw_on_key;
inline std::string loadStr(const std::string& section,const std::string& key,const std::string& fallback=""){auto i=values.find(section+":"+key);return i==values.end()?fallback:i->second;}
inline void saveStr(const std::string& section,const std::string& key,const std::string& value){if(key==throw_on_key)throw std::runtime_error("transport exception");values[section+":"+key]=value;++writes;if(auto_saving)++flushes;}
inline void saveInt(const std::string& section,const std::string& key,int value){saveStr(section,key,std::to_string(value));}
inline void saveFloat(const std::string& section,const std::string& key,float value){saveStr(section,key,std::to_string(value));}
inline void saveBool(const std::string& section,const std::string& key,bool value){saveStr(section,key,value?"yes":"no");}
inline void setAutoSave(bool on){auto_saving=on;if(on)++flushes;}
}
