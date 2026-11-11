// SPDX-License-Identifier: GPL-2.0-or-later
#include "dc_persistence.h"
#include "../atomic_file_io.h"
#include <mutex>
namespace silent_scope::dc_persistence {
namespace {std::mutex mutex;std::string error;}
bool save_config(const std::string& utf8_path,const std::string& serialized){
    std::lock_guard<std::mutex> lock(mutex);
    try{return write_file_atomic(std::filesystem::u8path(utf8_path),serialized,&error);}
    catch(...){error="Cannot save console settings";return false;}
}
std::string last_error(){std::lock_guard<std::mutex> lock(mutex);return error;}
void record_error(const std::string& message){std::lock_guard<std::mutex> lock(mutex);error=message;}
}
