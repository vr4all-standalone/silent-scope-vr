// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "menu_controller.h"
#include "shared/xr_panel.h"

namespace silent_scope {
struct MenuPointer {
    int row{-1};float u{.5f};glm::vec3 point{};int page_delta{};
};
// Silent Scope content/interaction mapping over the audited shared VR4ALL panel.
// The caller supplies the same room-space model to pointer() and draw().
class MenuPresentation {
public:
    static constexpr int rows_per_page=11;
    void update(const Settings&,MenuCapabilities,const MenuController&);
    const vr::xr::RowsPanel& panel() const{return panel_;}
    MenuPointer pointer(const glm::mat4& model,glm::vec3 origin,glm::vec3 direction) const;
    void draw(const glm::mat4& view_projection,const glm::mat4& model) const;
    // Footer hits return page_delta. Consume a fresh click in the host before
    // calling this, suppress that click in MenuController; never activate a row.
    void change_page(int delta);
    int page() const{return page_;}
    int page_count() const{return (menu_row_count+rows_per_page-1)/rows_per_page;}
    // Two distinct measured rates; this counter never implies native updates
    // match the headset's submitted-frame rate.
    void status_fps(double headset_hz,double native_hz);
    void save_error(const std::string& message);
    const std::string& status_text() const{return status_;}
private:
    vr::xr::RowsPanel panel_;
    std::array<int,rows_per_page> global_rows_{};
    std::array<MenuItem,menu_row_count> items_{};
    int page_{},last_selected_{-1};MenuRow selected_{MenuRow::Resume};
    bool open_{},help_{},confirm_{},fps_enabled_{};
    double headset_hz_{},native_hz_{};
    std::string error_,status_;
    void rebuild_panel();
    void rebuild_status();
};
}
