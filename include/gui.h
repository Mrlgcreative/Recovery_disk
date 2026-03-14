#pragma once

/*
 * gui.h
 * --------------------------------------------------------------------------
 * Interface graphique Dear ImGui pour le HDD Password Recovery Tool.
 *
 * Backend : DirectX11 + Win32 (natif Windows, zero dependance externe).
 * Reutilise DeviceScanner et ATAInterface existants.
 * --------------------------------------------------------------------------
 */

#include "ata_interface.h"
#include "ata_defs.h"

#include <string>
#include <vector>
#include <cstdint>

/* ==========================================================================
 *  GUI - Interface graphique principale
 * ========================================================================== */

class GUI {
public:
    /* ----- Panels ImGui (appeles depuis main_gui.cpp) ----- */
    void draw_menu_bar();
    void draw_device_list();
    void draw_device_details();
    void draw_security_panel();
    void draw_hex_viewer();
    void draw_status_bar();
    void draw_unlock_panel();

    /* ----- Actions ----- */
    void do_scan();
    void do_identify_raw();

private:

    /* ----- Etat ----- */
    std::vector<DeviceInfo> devices_;
    int selected_ = -1;
    bool show_hex_viewer_ = false;
    bool scan_done_ = false;

    /* Hex dump cache */
    bool hex_valid_ = false;
    uint8_t hex_data_[512]{};

    /* Messages de status */
    std::string status_msg_;
    float status_timer_ = 0.0f;

    /* ----- Helpers ----- */
    static const char* security_label(uint16_t status);
    static void security_color(uint16_t status, float out_rgb[3]);
};
