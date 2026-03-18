#pragma once

/*
 * gui.h
 * --------------------------------------------------------------------------
 * Interface graphique Dear ImGui pour le HDD Password Recovery Tool.
 *
 * Backend : DirectX11 + Win32 (natif Windows, zero dependance externe).
 * Docking layout avec panels : Disques, Details, Securite, Hex Viewer,
 * Detection constructeur, Deverrouillage, Backup SA, Mots de passe, Console.
 * --------------------------------------------------------------------------
 */

#include "ata_interface.h"
#include "ata_defs.h"
#include "sa_parser.h"

#include <string>
#include <vector>
#include <deque>
#include <cstdint>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

/* ==========================================================================
 *  LogEntry — entree du journal de la console integree
 * ========================================================================== */

struct LogEntry {
    enum Level { LVL_INFO, LVL_WARN, LVL_ERR, LVL_OK };
    Level       level;
    std::string message;
    std::string timestamp;   /* HH:MM:SS */
};

/* ==========================================================================
 *  GUI — Interface graphique principale (docking layout)
 * ========================================================================== */

/* Theme clair / sombre */
enum class AppTheme { DARK, LIGHT };

class GUI {
public:
    GUI();

    /* Rendu complet d'une frame (appele dans la boucle de rendu) */
    void render_frame();

    /* Actions (aussi declenchees par raccourcis) */
    void do_scan();

    /* Theme — appele une fois apres CreateContext */
    static void apply_theme();
    static void apply_theme(AppTheme theme);
    static AppTheme current_theme();

    /* Logo texture (chargee depuis main_gui.cpp) */
    static bool load_texture_from_file(const char* path,
                                       ID3D11Device* device,
                                       ID3D11ShaderResourceView** out_srv,
                                       int* out_w, int* out_h);
    static void set_logo_texture(ID3D11ShaderResourceView* srv, int w, int h);

private:

    /* ----- Panels ----- */
    void draw_dockspace();
    void draw_menu_bar();
    void draw_device_panel();
    void draw_details_panel();
    void draw_security_panel();
    void draw_hex_viewer();
    void draw_vendor_panel();
    void draw_unlock_panel();
    void draw_backup_panel();
    void draw_passwords_panel();
    void draw_log_console();
    void draw_status_bar();
    void draw_about_popup();

    /* ----- Actions internes ----- */
    void do_identify_raw();
    void do_detect_vendor();
    void do_unlock();
    void do_backup_sa();
    void do_extract_passwords();

    /* ----- Etat : Devices ----- */
    std::vector<DeviceInfo> devices_;
    int  selected_  = -1;
    bool scan_done_ = false;

    /* ----- Etat : Hex viewer ----- */
    bool    hex_valid_ = false;
    uint8_t hex_data_[512]{};

    /* ----- Etat : Detection constructeur ----- */
    bool        vendor_detected_ = false;
    std::string vendor_name_;

    /* ----- Etat : Deverrouillage ----- */
    enum class UnlockState { IDLE, SUCCESS, FAILED };
    UnlockState unlock_state_ = UnlockState::IDLE;
    std::string unlock_msg_;

    /* ----- Etat : Backup SA ----- */
    bool        backup_done_ = false;
    std::string backup_path_;
    std::string backup_sha_;

    /* ----- Etat : Mots de passe SA ----- */
    bool         passwords_extracted_ = false;
    PasswordInfo password_info_;
    bool         reveal_passwords_ = false;

    /* ----- Log console ----- */
    std::deque<LogEntry> log_;
    bool log_autoscroll_ = true;
    void log_msg(LogEntry::Level lvl, const std::string& msg);

    /* ----- Visibilite des panels ----- */
    bool show_hex_     = true;
    bool show_vendor_  = true;
    bool show_unlock_  = true;
    bool show_backup_  = true;
    bool show_pwd_     = true;
    bool show_log_     = true;
    bool show_about_   = false;

    /* ----- Status bar ----- */
    std::string status_msg_;
    float       status_timer_ = 0.0f;

    /* ----- Theme ----- */
    static inline AppTheme theme_ = AppTheme::DARK;

    /* ----- Logo texture ----- */
    static inline ID3D11ShaderResourceView* logo_srv_ = nullptr;
    static inline int logo_w_ = 0;
    static inline int logo_h_ = 0;

    /* ----- Helpers ----- */
    static const char* security_label(uint16_t status);
    static void security_color(uint16_t status, float out_rgb[3]);
};
