// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <limits>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace silent_scope {
// Exclusive same-directory temporary file, durable data and atomic replacement.
inline bool write_file_atomic(const std::filesystem::path& path,const std::string& data,std::string* error=nullptr) {
    if(error)error->clear();
    auto fail=[&](const char* message){if(error)*error=message;return false;};
    if(path.empty())return fail("Settings path is empty");
    static std::atomic<unsigned long> sequence{};
    auto temporary=path;
#ifdef _WIN32
    temporary+=L".tmp."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(sequence.fetch_add(1));
    HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return fail("Cannot create temporary settings file");
    bool okay=true;std::size_t position=0;
    while(position<data.size()) {
        DWORD written=0;const DWORD amount=DWORD((std::min)(data.size()-position,std::size_t(std::numeric_limits<DWORD>::max())));
        if(!WriteFile(file,data.data()+position,amount,&written,nullptr)||written==0){okay=false;break;}position+=written;
    }
    if(okay)okay=FlushFileBuffers(file)!=0;
    if(!CloseHandle(file))okay=false;
    if(okay)okay=MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!okay){DeleteFileW(temporary.c_str());return fail("Cannot write or atomically replace settings file");}
#else
    temporary+=".tmp."+std::to_string(getpid())+"."+std::to_string(sequence.fetch_add(1));
    const int file=::open(temporary.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);
    if(file<0)return fail("Cannot create temporary settings file");
    bool okay=true;std::size_t position=0;
    while(position<data.size()) {
        const auto written=::write(file,data.data()+position,data.size()-position);
        if(written<0&&errno==EINTR)continue;
        if(written<=0){okay=false;break;}position+=std::size_t(written);
    }
    if(okay)okay=::fsync(file)==0;
    if(::close(file)!=0)okay=false;
    if(okay)okay=::rename(temporary.c_str(),path.c_str())==0;
    if(!okay){::unlink(temporary.c_str());return fail("Cannot write or atomically replace settings file");}
    auto parent=path.parent_path();if(parent.empty())parent=".";
    const int directory=::open(parent.c_str(),O_RDONLY|O_DIRECTORY);
    if(directory>=0){::fsync(directory);::close(directory);}
#endif
    return true;
}
}
