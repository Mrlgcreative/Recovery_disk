/*
 * gui.cpp
 * --------------------------------------------------------------------------
 * Implementation de l'interface graphique Dear ImGui.
 *
 * Tous les panels sont dessines en immediate-mode :
 *   - Liste des disques (tableau interactif)
 *   - Details du disque selectionne
 *   - Analyse de securite ATA (flags colores)
 *   - Hex viewer pour IDENTIFY DEVICE brut
 *   - Panel de deverrouillage (futur sprint)
 * --------------------------------------------------------------------------
 */

#include "gui.h"
#include "cli.h"  /* DeviceScanner */

#include "imgui.h"

#include <cstdio>
#include <cstring>

/* ==========================================================================
 *  Helpers
 * ========================================================================== */

const char* GUI::security_label(uint16_t s) {
    if (SEC_IS_LOCKED(s))                     return "LOCKED";
    if (SEC_IS_FROZEN(s))                     return "FROZEN";
    if (SEC_IS_ENABLED(s))                    return "ENABLED";
    if (s & SEC_FLAG_SUPPORTED)               return "OK";
    return "---";
}

void GUI::security_color(uint16_t s, float rgb[3]) {
    if (SEC_IS_LOCKED(s))      { rgb[0]=1.0f; rgb[1]=0.2f; rgb[2]=0.2f; return; }
    if (SEC_IS_FROZEN(s))      { rgb[0]=1.0f; rgb[1]=0.8f; rgb[2]=0.0f; return; }
    if (SEC_IS_ENABLED(s))     { rgb[0]=0.8f; rgb[1]=0.4f; rgb[2]=0.8f; return; }
    if (s & SEC_FLAG_SUPPORTED){ rgb[0]=0.2f; rgb[1]=0.8f; rgb[2]=0.2f; return; }
    rgb[0]=0.5f; rgb[1]=0.5f; rgb[2]=0.5f;
}

/* ==========================================================================
 *  Actions
 * ========================================================================== */

void GUI::do_scan() {
    auto paths = DeviceScanner::scan();
    devices_.clear();
    selected_ = -1;
    hex_valid_ = false;

    for (const auto& p : paths) {
        DeviceInfo info{};
        if (DeviceScanner::probe_device(p, info))
            devices_.push_back(info);
    }

    scan_done_ = true;

    if (!devices_.empty())
        selected_ = 0;

    char msg[128];
    snprintf(msg, sizeof(msg), "%zu disque(s) detecte(s).", devices_.size());
    status_msg_ = msg;
    status_timer_ = 3.0f;
}

void GUI::do_identify_raw() {
    hex_valid_ = false;
    if (selected_ < 0) return;

    const auto& dev = devices_[selected_];
    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(dev.path);
    if (err != ATAError::OK) {
        status_msg_ = "Erreur: impossible d'ouvrir le disque.";
        status_timer_ = 3.0f;
        return;
    }

    IdentifyData id{};
    err = h->identify_device(id);
    if (err != ATAError::OK) {
        status_msg_ = "IDENTIFY DEVICE echoue (disque peut-etre verrouille).";
        status_timer_ = 3.0f;
        return;
    }

    std::memcpy(hex_data_, &id, 512);
    hex_valid_ = true;
    show_hex_viewer_ = true;
}

/* ==========================================================================
 *  Panels ImGui
 * ========================================================================== */

void GUI::draw_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Fichier")) {
            if (ImGui::MenuItem("Scanner les disques", "F5"))
                do_scan();
            ImGui::Separator();
            if (ImGui::MenuItem("Quitter", "Alt+F4"))
                std::exit(0);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Affichage")) {
            ImGui::MenuItem("Hex Viewer", nullptr, &show_hex_viewer_);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void GUI::draw_device_list() {
    ImGui::Begin("Disques detectes", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button("Scanner (F5)")) do_scan();
    ImGui::SameLine();
    ImGui::Text("%zu disque(s)", devices_.size());

    ImGui::Separator();

    if (!devices_.empty() &&
        ImGui::BeginTable("##devices", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
            ImVec2(0, 0)))
    {
        ImGui::TableSetupColumn("#",        ImGuiTableColumnFlags_WidthFixed, 30.0f);
        ImGui::TableSetupColumn("Modele",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Serial",   ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Firmware", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Securite", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(devices_.size()); ++i) {
            const auto& d = devices_[i];
            ImGui::TableNextRow();

            /* Colonne # */
            ImGui::TableSetColumnIndex(0);
            char label[16];
            snprintf(label, sizeof(label), "%d", i);
            bool is_sel = (selected_ == i);
            if (ImGui::Selectable(label, is_sel,
                    ImGuiSelectableFlags_SpanAllColumns)) {
                selected_ = i;
                hex_valid_ = false;
            }

            /* Colonne Modele */
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(d.model);

            /* Colonne Serial */
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(d.serial);

            /* Colonne Firmware */
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(d.firmware);

            /* Colonne Securite */
            ImGui::TableSetColumnIndex(4);
            float rgb[3];
            security_color(d.security_status, rgb);
            ImGui::TextColored(ImVec4(rgb[0], rgb[1], rgb[2], 1.0f),
                               "%s", security_label(d.security_status));
        }

        ImGui::EndTable();
    }
    else if (devices_.empty() && scan_done_) {
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1),
            "Aucun disque detecte.");
        ImGui::Text("Verifiez les droits Administrateur.");
    }
    else if (!scan_done_) {
        ImGui::Text("Cliquez sur 'Scanner' pour detecter les disques.");
    }

    ImGui::End();
}

void GUI::draw_device_details() {
    ImGui::Begin("Details du disque", nullptr, ImGuiWindowFlags_NoCollapse);

    if (selected_ < 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1),
            "Selectionnez un disque dans la liste.");
        ImGui::End();
        return;
    }

    const auto& d = devices_[selected_];

    ImGui::Text("Chemin   : %s", d.path);
    ImGui::Text("Modele   : %s", d.model);
    ImGui::Text("Serial   : %s", d.serial);
    ImGui::Text("Firmware : %s", d.firmware);

    ImGui::Separator();

    char sec_str[64];
    ata_security_status_str(d.security_status, sec_str);
    ImGui::Text("Securite : %s (word 128 = 0x%04X)", sec_str, d.security_status);

    ImGui::Separator();

    if (ImGui::Button("Lire IDENTIFY brut")) {
        do_identify_raw();
    }

    ImGui::End();
}

void GUI::draw_security_panel() {
    ImGui::Begin("Analyse de securite ATA", nullptr, ImGuiWindowFlags_NoCollapse);

    if (selected_ < 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1),
            "Selectionnez un disque.");
        ImGui::End();
        return;
    }

    const auto& d = devices_[selected_];
    uint16_t s = d.security_status;

    ImGui::Text("Disque : %s", d.model);
    ImGui::Text("Word 128 brut : 0x%04X", s);

    ImGui::Separator();

    /* Flags individuels avec icones couleur */
    struct FlagEntry {
        const char* name;
        uint16_t    mask;
        const char* desc;
    };
    static const FlagEntry flags[] = {
        {"SUPPORTED",      SEC_FLAG_SUPPORTED,      "Security Feature Set disponible"},
        {"ENABLED",        SEC_FLAG_ENABLED,        "Un password est defini"},
        {"LOCKED",         SEC_FLAG_LOCKED,         "Disque verrouille - acces bloque"},
        {"FROZEN",         SEC_FLAG_FROZEN,         "Securite gelee par BIOS/OS"},
        {"COUNT_EXPIRED",  SEC_FLAG_COUNT_EXPIRED,  "Trop de tentatives echouees"},
        {"ENHANCED_ERASE", SEC_FLAG_ENHANCED_ERASE, "Enhanced Secure Erase disponible"},
    };

    for (const auto& f : flags) {
        bool active = (s & f.mask) != 0;
        ImVec4 col = active ? ImVec4(0.2f, 0.9f, 0.2f, 1) : ImVec4(0.5f, 0.5f, 0.5f, 1);

        ImGui::TextColored(col, "[%s]", active ? "X" : " ");
        ImGui::SameLine();
        ImGui::Text("%-18s %s", f.name, f.desc);
    }

    ImGui::Separator();

    /* Verdict */
    ImGui::Text("Verdict :");
    ImGui::SameLine();

    if (!(s & SEC_FLAG_SUPPORTED)) {
        ImGui::TextColored(ImVec4(0.2f,0.8f,0.2f,1),
            "Pas de securite ATA sur ce disque.");
    } else if (SEC_IS_EXPIRED(s)) {
        ImGui::TextColored(ImVec4(1,0.2f,0.2f,1),
            "BLOQUE - Compteur de tentatives expire. Power cycle requis.");
    } else if (SEC_IS_LOCKED(s) && SEC_IS_FROZEN(s)) {
        ImGui::TextColored(ImVec4(1,0.2f,0.2f,1),
            "VERROUILLE + GELE - Suspend-to-RAM ou hot-swap necessaire.");
    } else if (SEC_IS_LOCKED(s)) {
        ImGui::TextColored(ImVec4(1,0.2f,0.2f,1),
            "VERROUILLE - Password requis pour deverrouillage.");
    } else if (SEC_IS_FROZEN(s)) {
        ImGui::TextColored(ImVec4(1,0.8f,0,1),
            "GELE - Suspend-to-RAM pour degeler.");
    } else if (SEC_IS_ENABLED(s)) {
        ImGui::TextColored(ImVec4(0.8f,0.4f,0.8f,1),
            "PASSWORD ACTIF mais disque non verrouille.");
    } else {
        ImGui::TextColored(ImVec4(0.2f,0.8f,0.2f,1),
            "Securite supportee, aucun password actif.");
    }

    ImGui::End();
}

void GUI::draw_hex_viewer() {
    if (!show_hex_viewer_) return;

    ImGui::Begin("IDENTIFY DEVICE - Hex Viewer", &show_hex_viewer_);

    if (!hex_valid_) {
        ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1),
            "Appuyez sur 'Lire IDENTIFY brut' dans les Details.");
        ImGui::End();
        return;
    }

    /* Header */
    ImGui::Text("Offset ");
    ImGui::SameLine(70);
    for (int c = 0; c < 16; ++c) {
        ImGui::SameLine(70.0f + c * 25.0f);
        ImGui::Text("%02X", c);
    }
    ImGui::SameLine(70.0f + 16 * 25.0f + 10.0f);
    ImGui::Text("ASCII");

    ImGui::Separator();

    /* Utiliser une police monospace et un child scrollable */
    ImGui::BeginChild("##hexchild", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (int row = 0; row < 32; ++row) {
        /* Offset */
        ImGui::Text("%04X  ", row * 16);

        /* Hex bytes */
        for (int col = 0; col < 16; ++col) {
            ImGui::SameLine(70.0f + col * 25.0f);
            int off = row * 16 + col;
            ImGui::Text("%02X", hex_data_[off]);
        }

        /* ASCII */
        ImGui::SameLine(70.0f + 16 * 25.0f + 10.0f);
        char ascii[17];
        for (int col = 0; col < 16; ++col) {
            uint8_t b = hex_data_[row * 16 + col];
            ascii[col] = (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';
        }
        ascii[16] = '\0';
        ImGui::TextUnformatted(ascii);
    }

    ImGui::EndChild();
    ImGui::End();
}

void GUI::draw_unlock_panel() {
    if (selected_ < 0) return;
    const auto& d = devices_[selected_];

    if (!SEC_IS_LOCKED(d.security_status)) return;

    ImGui::Begin("Deverrouillage", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextColored(ImVec4(1,0.3f,0.3f,1),
        "Ce disque est VERROUILLE.");
    ImGui::Separator();
    ImGui::Text("Fonctionnalite de deverrouillage en cours de developpement.");
    ImGui::Text("Sprints futurs : VSC password recovery, RAM patching.");

    ImGui::End();
}

void GUI::draw_status_bar() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));

    ImGui::Begin("##statusbar", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);

    if (status_timer_ > 0) {
        ImGui::TextUnformatted(status_msg_.c_str());
        float dt = ImGui::GetIO().DeltaTime;
        status_timer_ -= dt;
    } else {
        ImGui::Text("Pret | %zu disque(s)", devices_.size());
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

/* Note : la creation de la fenetre Win32 + DX11 est dans main_gui.cpp
 * qui appelle les methodes draw_*() dans la boucle de rendu. */
