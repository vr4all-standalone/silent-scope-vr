// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <string>
namespace silent_scope::dc_persistence {
bool save_config(const std::string& utf8_path,const std::string& serialized);
std::string last_error();
void record_error(const std::string& message);
}
