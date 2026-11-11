// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "settings.h"
#include "atomic_file_io.h"
#include <fstream>
namespace silent_scope {
inline Settings load_settings_file(const std::filesystem::path& path) {
    std::ifstream in(path);return Settings::load(in);
}
inline bool save_settings_atomic(const std::filesystem::path& path,const Settings& settings,std::string* error=nullptr) {
    std::ostringstream serialized;settings.save(serialized);
    return write_file_atomic(path,serialized.str(),error);
}
}
