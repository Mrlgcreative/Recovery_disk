/*
 * gui.cpp
 * --------------------------------------------------------------------------
 * Implementation de l'interface graphique Dear ImGui — docking layout.
 *
 * Panels :
 *   - Disques         : liste cliquable + scan
 *   - Details         : infos du disque selectionne
 *   - Securite ATA    : flags colores, verdict
 *   - Hex Viewer      : dump IDENTIFY brut 512 bytes
 *   - Constructeur    : detection vendor (VSC)
 *   - Deverrouillage  : workflow RAM patch / password
 *   - Backup SA       : sauvegarde Service Area
 *   - Mots de passe   : extraction passwords SA
 *   - Console         : journal des evenements
 *   - Status bar      : message en bas
 * --------------------------------------------------------------------------
 */

#include "gui.h"
#include "cli.h"
#include "vsc_engine.h"
#include "ram_patcher.h"
#include "sa_backup.h"
#include "device.h"
#include "config.h"

#include "imgui.h"
#include "imgui_internal.h"   /* DockBuilder API */

#include <cstdio>
#include <cstring>
#include <ctime>
#include <algorithm>

#ifdef _WIN32
#include <d3d11.h>
#include <wincodec.h>   /* WIC — chargement PNG natif Windows */
#pragma comment(lib, "windowscodecs.lib")
#endif

/* ==========================================================================
 *  Constructeur
 * ========================================================================== */

GUI::GUI() {
    log_msg(LogEntry::LVL_INFO, "HDD Password Recovery Tool v1.0.0");
    log_msg(LogEntry::LVL_INFO, "Interface graphique initialisee.");
}

/* ==========================================================================
 *  Helpers
 * ========================================================================== */

static const char* timestamp_now() {
    static char buf[16];
    time_t t = time(nullptr);
    struct tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", lt.tm_hour, lt.tm_min, lt.tm_sec);
    return buf;
}

void GUI::log_msg(LogEntry::Level lvl, const std::string& msg) {
    LogEntry e;
    e.level     = lvl;
    e.message   = msg;
    e.timestamp = timestamp_now();
    log_.push_back(std::move(e));
    if (log_.size() > 500) log_.pop_front();
}

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
 *  Theme
 * ========================================================================== */

void GUI::apply_theme() {
    ImGuiStyle& s = ImGui::GetStyle();

    /* Geometry */
    s.WindowRounding    = 4.0f;
    s.FrameRounding     = 3.0f;
    s.GrabRounding      = 3.0f;
    s.ScrollbarRounding = 4.0f;
    s.TabRounding       = 3.0f;
    s.ChildRounding     = 3.0f;
    s.PopupRounding     = 4.0f;
    s.WindowPadding     = ImVec2(10, 10);
    s.FramePadding      = ImVec2(8, 4);
    s.ItemSpacing       = ImVec2(8, 6);
    s.ScrollbarSize     = 14.0f;
    s.GrabMinSize       = 12.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBorderSize     = 0.0f;

    ImVec4* c = s.Colors;

    /* Background */
    c[ImGuiCol_WindowBg]             = ImVec4(0.09f, 0.09f, 0.12f, 1.00f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.11f, 0.11f, 0.15f, 0.96f);

    /* Borders */
    c[ImGuiCol_Border]               = ImVec4(0.20f, 0.22f, 0.30f, 0.60f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    /* Frame */
    c[ImGuiCol_FrameBg]              = ImVec4(0.14f, 0.14f, 0.19f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.18f, 0.25f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.22f, 0.22f, 0.30f, 1.00f);

    /* Title */
    c[ImGuiCol_TitleBg]              = ImVec4(0.07f, 0.07f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.10f, 0.12f, 0.18f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.07f, 0.07f, 0.10f, 0.60f);

    /* Tabs */
    c[ImGuiCol_Tab]                  = ImVec4(0.12f, 0.12f, 0.17f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.22f, 0.28f, 0.45f, 1.00f);
    c[ImGuiCol_TabSelected]          = ImVec4(0.18f, 0.22f, 0.36f, 1.00f);
    c[ImGuiCol_TabDimmed]            = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
    c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.14f, 0.16f, 0.24f, 1.00f);

    /* Header (collapsing, selectable) */
    c[ImGuiCol_Header]               = ImVec4(0.16f, 0.18f, 0.28f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.22f, 0.26f, 0.40f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.20f, 0.24f, 0.38f, 1.00f);

    /* Buttons */
    c[ImGuiCol_Button]               = ImVec4(0.16f, 0.20f, 0.34f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.24f, 0.30f, 0.50f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.20f, 0.26f, 0.44f, 1.00f);

    /* Separator */
    c[ImGuiCol_Separator]            = ImVec4(0.22f, 0.24f, 0.32f, 0.60f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.30f, 0.40f, 0.64f, 1.00f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.36f, 0.46f, 0.72f, 1.00f);

    /* Resize grip */
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.22f, 0.28f, 0.44f, 0.40f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.30f, 0.38f, 0.60f, 0.70f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.36f, 0.44f, 0.68f, 0.90f);

    /* Scrollbar */
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.08f, 0.10f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.22f, 0.24f, 0.32f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.30f, 0.40f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.34f, 0.36f, 0.48f, 1.00f);

    /* Checkmark, slider */
    c[ImGuiCol_CheckMark]            = ImVec4(0.40f, 0.60f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.30f, 0.44f, 0.80f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.36f, 0.50f, 0.90f, 1.00f);

    /* Text */
    c[ImGuiCol_Text]                 = ImVec4(0.88f, 0.90f, 0.95f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.42f, 0.44f, 0.50f, 1.00f);

    /* Docking */
    c[ImGuiCol_DockingPreview]       = ImVec4(0.24f, 0.36f, 0.64f, 0.70f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.06f, 0.06f, 0.08f, 1.00f);

    /* MenuBar */
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);

    /* Table */
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.14f, 0.16f, 0.22f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.20f, 0.22f, 0.30f, 1.00f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.16f, 0.18f, 0.24f, 0.60f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.10f, 0.10f, 0.14f, 0.40f);
}

/* --------------------------------------------------------------------------
 *  Theme clair
 * -------------------------------------------------------------------------- */

static void apply_light_colors() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    /* Background */
    c[ImGuiCol_WindowBg]             = ImVec4(0.95f, 0.95f, 0.96f, 1.00f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.98f, 0.98f, 0.98f, 0.96f);

    /* Borders */
    c[ImGuiCol_Border]               = ImVec4(0.72f, 0.72f, 0.75f, 0.60f);
    c[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    /* Frame */
    c[ImGuiCol_FrameBg]              = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.82f, 0.84f, 0.88f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.76f, 0.78f, 0.84f, 1.00f);

    /* Title */
    c[ImGuiCol_TitleBg]              = ImVec4(0.86f, 0.86f, 0.90f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.78f, 0.80f, 0.88f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.90f, 0.90f, 0.92f, 0.60f);

    /* Tabs */
    c[ImGuiCol_Tab]                  = ImVec4(0.86f, 0.86f, 0.90f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.62f, 0.68f, 0.82f, 1.00f);
    c[ImGuiCol_TabSelected]          = ImVec4(0.70f, 0.74f, 0.86f, 1.00f);
    c[ImGuiCol_TabDimmed]            = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.82f, 0.84f, 0.90f, 1.00f);

    /* Header (collapsing, selectable) */
    c[ImGuiCol_Header]               = ImVec4(0.76f, 0.78f, 0.86f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.68f, 0.72f, 0.84f, 1.00f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.64f, 0.68f, 0.82f, 1.00f);

    /* Buttons */
    c[ImGuiCol_Button]               = ImVec4(0.68f, 0.72f, 0.84f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.56f, 0.62f, 0.80f, 1.00f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.50f, 0.56f, 0.76f, 1.00f);

    /* Separator */
    c[ImGuiCol_Separator]            = ImVec4(0.72f, 0.72f, 0.75f, 0.60f);
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.46f, 0.54f, 0.74f, 1.00f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.40f, 0.48f, 0.70f, 1.00f);

    /* Resize grip */
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.60f, 0.64f, 0.76f, 0.40f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.50f, 0.56f, 0.72f, 0.70f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.44f, 0.50f, 0.68f, 0.90f);

    /* Scrollbar */
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.90f, 0.90f, 0.92f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.70f, 0.72f, 0.78f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.62f, 0.64f, 0.72f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.54f, 0.56f, 0.66f, 1.00f);

    /* Checkmark, slider */
    c[ImGuiCol_CheckMark]            = ImVec4(0.24f, 0.42f, 0.76f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.30f, 0.46f, 0.78f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.26f, 0.40f, 0.72f, 1.00f);

    /* Text */
    c[ImGuiCol_Text]                 = ImVec4(0.10f, 0.10f, 0.14f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.48f, 0.48f, 0.54f, 1.00f);

    /* Docking */
    c[ImGuiCol_DockingPreview]       = ImVec4(0.40f, 0.54f, 0.80f, 0.70f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);

    /* MenuBar */
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.88f, 0.88f, 0.92f, 1.00f);

    /* Table */
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.82f, 0.84f, 0.90f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.72f, 0.72f, 0.78f, 1.00f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.80f, 0.80f, 0.84f, 0.60f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(0.88f, 0.88f, 0.92f, 0.40f);
}

/* --------------------------------------------------------------------------
 *  apply_theme(AppTheme) — bascule entre clair et sombre
 * -------------------------------------------------------------------------- */

void GUI::apply_theme(AppTheme theme) {
    theme_ = theme;

    /* Geometrie commune */
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 4.0f;
    s.FrameRounding     = 3.0f;
    s.GrabRounding      = 3.0f;
    s.ScrollbarRounding = 4.0f;
    s.TabRounding       = 3.0f;
    s.ChildRounding     = 3.0f;
    s.PopupRounding     = 4.0f;
    s.WindowPadding     = ImVec2(10, 10);
    s.FramePadding      = ImVec2(8, 4);
    s.ItemSpacing       = ImVec2(8, 6);
    s.ScrollbarSize     = 14.0f;
    s.GrabMinSize       = 12.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBorderSize     = 0.0f;

    if (theme == AppTheme::LIGHT)
        apply_light_colors();
    else
        apply_theme();   /* reutilise le dark existant */
}

AppTheme GUI::current_theme() {
    return theme_;
}

/* ==========================================================================
 *  Logo texture (PNG → DX11 ShaderResourceView via WIC)
 * ========================================================================== */

#ifdef _WIN32
bool GUI::load_texture_from_file(const char* path,
                                 ID3D11Device* device,
                                 ID3D11ShaderResourceView** out_srv,
                                 int* out_w, int* out_h)
{
    *out_srv = nullptr;
    *out_w = *out_h = 0;

    /* Convertir chemin en wide string */
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wlen <= 0) return false;
    wchar_t* wpath = (wchar_t*)_alloca(wlen * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wlen);

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    IWICImagingFactory* wicFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
    if (FAILED(hr)) return false;

    IWICBitmapDecoder* decoder = nullptr;
    hr = wicFactory->CreateDecoderFromFilename(wpath, nullptr,
        GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr)) { wicFactory->Release(); return false; }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) { decoder->Release(); wicFactory->Release(); return false; }

    IWICFormatConverter* converter = nullptr;
    hr = wicFactory->CreateFormatConverter(&converter);
    if (FAILED(hr)) { frame->Release(); decoder->Release(); wicFactory->Release(); return false; }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) { converter->Release(); frame->Release(); decoder->Release(); wicFactory->Release(); return false; }

    UINT w = 0, h = 0;
    converter->GetSize(&w, &h);

    BYTE* pixels = new BYTE[w * h * 4];
    hr = converter->CopyPixels(nullptr, w * 4, w * h * 4, pixels);

    converter->Release();
    frame->Release();
    decoder->Release();
    wicFactory->Release();

    if (FAILED(hr)) { delete[] pixels; return false; }

    /* Creer la texture DX11 */
    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = w;
    td.Height           = h;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_DEFAULT;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem     = pixels;
    initData.SysMemPitch = w * 4;

    ID3D11Texture2D* tex = nullptr;
    hr = device->CreateTexture2D(&td, &initData, &tex);
    delete[] pixels;
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                    = td.Format;
    srvDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels       = 1;

    hr = device->CreateShaderResourceView(tex, &srvDesc, out_srv);
    tex->Release();
    if (FAILED(hr)) return false;

    *out_w = (int)w;
    *out_h = (int)h;
    return true;
}
#else
bool GUI::load_texture_from_file(const char*, ID3D11Device*,
                                 ID3D11ShaderResourceView**, int*, int*)
{ return false; }
#endif

void GUI::set_logo_texture(ID3D11ShaderResourceView* srv, int w, int h) {
    logo_srv_ = srv;
    logo_w_   = w;
    logo_h_   = h;
}

/* ==========================================================================
 *  Actions
 * ========================================================================== */

void GUI::do_scan() {
    log_msg(LogEntry::LVL_INFO, "Scan des disques physiques...");
    auto paths = DeviceScanner::scan();

    devices_.clear();
    selected_ = -1;
    hex_valid_ = false;
    vendor_detected_ = false;
    vendor_name_.clear();
    unlock_state_ = UnlockState::IDLE;
    backup_done_ = false;
    passwords_extracted_ = false;

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
    log_msg(devices_.empty() ? LogEntry::LVL_WARN : LogEntry::LVL_OK, msg);
    status_msg_ = msg;
    status_timer_ = 4.0f;
}

void GUI::do_identify_raw() {
    hex_valid_ = false;
    if (selected_ < 0) return;

    const auto& dev = devices_[selected_];
    log_msg(LogEntry::LVL_INFO, std::string("IDENTIFY DEVICE sur ") + dev.path);

    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(dev.path);
    if (err != ATAError::OK) {
        log_msg(LogEntry::LVL_ERR, std::string("Erreur ouverture : ") + ata_error_str(err));
        return;
    }

    IdentifyData id{};
    err = h->identify_device(id);
    if (err != ATAError::OK) {
        log_msg(LogEntry::LVL_ERR, "IDENTIFY DEVICE echoue (disque peut-etre verrouille).");
        return;
    }

    std::memcpy(hex_data_, &id, 512);
    hex_valid_ = true;
    show_hex_  = true;
    log_msg(LogEntry::LVL_OK, "IDENTIFY DEVICE lu avec succes (512 bytes).");
}

void GUI::do_detect_vendor() {
    vendor_detected_ = false;
    vendor_name_.clear();
    if (selected_ < 0) return;

    const auto& dev = devices_[selected_];
    log_msg(LogEntry::LVL_INFO, std::string("Detection constructeur pour ") + dev.model);

    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(dev.path);
    if (err != ATAError::OK) {
        log_msg(LogEntry::LVL_ERR, std::string("Erreur ouverture : ") + ata_error_str(err));
        return;
    }

    IdentifyData id{};
    err = h->identify_device(id);
    if (err != ATAError::OK) {
        log_msg(LogEntry::LVL_ERR, "IDENTIFY DEVICE echoue.");
        return;
    }

    VSCEngine engine(h.get());
    auto handler = engine.detect_vendor(id);

    if (handler) {
        vendor_detected_ = true;
        vendor_name_ = handler->vendor_name();
        log_msg(LogEntry::LVL_OK, "Constructeur detecte : " + vendor_name_);
    } else {
        log_msg(LogEntry::LVL_WARN, "Constructeur non reconnu — commandes VSC indisponibles.");
    }
}

void GUI::do_unlock() {
    unlock_state_ = UnlockState::IDLE;
    unlock_msg_.clear();
    if (selected_ < 0) return;

    const auto& dev = devices_[selected_];
    log_msg(LogEntry::LVL_INFO, std::string("Tentative de deverrouillage : ") + dev.model);

    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(dev.path);
    if (err != ATAError::OK) {
        unlock_state_ = UnlockState::FAILED;
        unlock_msg_ = std::string("Erreur ouverture : ") + ata_error_str(err);
        log_msg(LogEntry::LVL_ERR, unlock_msg_);
        return;
    }

    IdentifyData id{};
    h->identify_device(id);

    Device device(h.get());
    device.set_identify(id);

    VSCEngine engine(h.get());
    RAMPatcher patcher(h.get(), engine);
    UnlockResult result = patcher.attempt_unlock(device);

    unlock_msg_ = unlock_result_str(result);
    backup_path_ = patcher.last_backup_path();
    password_info_ = patcher.last_password_info();

    if (result == UnlockResult::SUCCESS || result == UnlockResult::PASSWORD_UNLOCK) {
        unlock_state_ = UnlockState::SUCCESS;
        log_msg(LogEntry::LVL_OK, "Deverrouillage REUSSI : " + unlock_msg_);
        if (!backup_path_.empty()) {
            backup_done_ = true;
            log_msg(LogEntry::LVL_INFO, "Backup SA : " + backup_path_);
        }
        if (password_info_.found) {
            passwords_extracted_ = true;
            log_msg(LogEntry::LVL_OK, "Mots de passe SA extraits.");
        }
    } else {
        unlock_state_ = UnlockState::FAILED;
        log_msg(LogEntry::LVL_ERR, "Deverrouillage ECHOUE : " + unlock_msg_);
    }
}

void GUI::do_backup_sa() {
    backup_done_ = false;
    backup_path_.clear();
    backup_sha_.clear();
    if (selected_ < 0) return;

    const auto& dev = devices_[selected_];
    log_msg(LogEntry::LVL_INFO, std::string("Backup SA pour ") + dev.model);

    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(dev.path);
    if (err != ATAError::OK) {
        log_msg(LogEntry::LVL_ERR, std::string("Erreur ouverture : ") + ata_error_str(err));
        return;
    }

    IdentifyData id{};
    h->identify_device(id);

    VSCEngine engine(h.get());
    auto handler = engine.detect_vendor(id);
    if (!handler) {
        log_msg(LogEntry::LVL_ERR, "Constructeur non reconnu — backup impossible.");
        return;
    }

    log_msg(LogEntry::LVL_INFO, std::string("Constructeur : ") + handler->vendor_name());

    std::string path = SABackup::backup_sa(handler.get(), dev.serial, dev.model);
    if (path.empty()) {
        log_msg(LogEntry::LVL_ERR, "Backup SA echoue.");
        return;
    }

    backup_done_ = true;
    backup_path_ = path;
    backup_sha_  = SABackup::sha256_file(path);

    log_msg(LogEntry::LVL_OK, "Backup SA cree : " + path);
    if (!backup_sha_.empty())
        log_msg(LogEntry::LVL_INFO, "SHA-256 : " + backup_sha_);
}

void GUI::do_extract_passwords() {
    passwords_extracted_ = false;
    password_info_ = {};
    if (selected_ < 0) return;

    const auto& dev = devices_[selected_];
    log_msg(LogEntry::LVL_INFO, std::string("Extraction mots de passe SA pour ") + dev.model);

    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(dev.path);
    if (err != ATAError::OK) {
        log_msg(LogEntry::LVL_ERR, std::string("Erreur ouverture : ") + ata_error_str(err));
        return;
    }

    IdentifyData id{};
    h->identify_device(id);

    VSCEngine engine(h.get());
    auto handler = engine.detect_vendor(id);
    if (!handler) {
        log_msg(LogEntry::LVL_ERR, "Constructeur non reconnu — extraction impossible.");
        return;
    }

    if (!handler->enter_vendor_mode()) {
        log_msg(LogEntry::LVL_ERR, "Impossible d'entrer en mode vendor.");
        return;
    }
    VendorModeGuard guard(handler.get());

    uint8_t module_buf[512]{};
    if (!handler->read_sa_module(WD_SA_MODULE_PASSWORD,
                                 std::span<uint8_t>(module_buf, 512))) {
        log_msg(LogEntry::LVL_ERR, "Lecture du module password SA echouee.");
        return;
    }

    password_info_ = SAParser::parse_wd_password_module(module_buf, 512);
    passwords_extracted_ = true;

    if (password_info_.found) {
        log_msg(LogEntry::LVL_OK, "Mots de passe extraits avec succes.");
        if (password_info_.is_empty_password)
            log_msg(LogEntry::LVL_INFO, "Note : password vide (tous zeros).");
    } else {
        log_msg(LogEntry::LVL_WARN, "Module password lu mais aucun password trouve.");
    }
}

/* ==========================================================================
 *  Render Frame — appele une fois par frame
 * ========================================================================== */

void GUI::render_frame() {
    draw_dockspace();
    draw_menu_bar();
    draw_device_panel();
    draw_details_panel();
    draw_security_panel();

    if (show_hex_)     draw_hex_viewer();
    if (show_vendor_)  draw_vendor_panel();
    if (show_unlock_)  draw_unlock_panel();
    if (show_backup_)  draw_backup_panel();
    if (show_pwd_)     draw_passwords_panel();
    if (show_log_)     draw_log_console();

    draw_status_bar();

    if (show_about_)   draw_about_popup();
}

/* ==========================================================================
 *  Dockspace — layout plein ecran
 * ========================================================================== */

void GUI::draw_dockspace() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float bar_h = ImGui::GetFrameHeight();

    /* Fenetre invisible couvrant tout le viewport (sous la status bar) */
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, vp->WorkSize.y - bar_h));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##DockHost", nullptr, flags);
    ImGui::PopStyleVar(3);

    ImGuiID dock_id = ImGui::GetID("MainDock");

    /* Construction du layout initial (une seule fois) */
    static bool layout_built = false;
    if (!layout_built) {
        layout_built = true;

        ImGui::DockBuilderRemoveNode(dock_id);
        ImGui::DockBuilderAddNode(dock_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock_id, ImVec2(vp->WorkSize.x, vp->WorkSize.y - bar_h));

        /* Split : gauche 22% | reste */
        ImGuiID dock_left, dock_main;
        ImGui::DockBuilderSplitNode(dock_id, ImGuiDir_Left, 0.22f, &dock_left, &dock_main);

        /* Split reste : centre | droite 32% */
        ImGuiID dock_center, dock_right;
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.35f, &dock_right, &dock_center);

        /* Split centre : haut | bas 28% (console) */
        ImGuiID dock_center_top, dock_bottom;
        ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Down, 0.28f, &dock_bottom, &dock_center_top);

        /* Assigner les panels */
        ImGui::DockBuilderDockWindow("Disques",           dock_left);
        ImGui::DockBuilderDockWindow("Details",           dock_center_top);
        ImGui::DockBuilderDockWindow("Securite ATA",      dock_center_top);
        ImGui::DockBuilderDockWindow("Hex Viewer",        dock_center_top);
        ImGui::DockBuilderDockWindow("Constructeur",      dock_right);
        ImGui::DockBuilderDockWindow("Deverrouillage",    dock_right);
        ImGui::DockBuilderDockWindow("Backup SA",         dock_right);
        ImGui::DockBuilderDockWindow("Mots de passe SA",  dock_right);
        ImGui::DockBuilderDockWindow("Console",           dock_bottom);

        ImGui::DockBuilderFinish(dock_id);
    }

    ImGui::DockSpace(dock_id, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}

/* ==========================================================================
 *  Menu Bar
 * ========================================================================== */

void GUI::draw_menu_bar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("Fichier")) {
        if (ImGui::MenuItem("Scanner les disques", "F5"))
            do_scan();
        ImGui::Separator();
        if (ImGui::MenuItem("Quitter", "Alt+F4"))
            std::exit(0);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Affichage")) {
        ImGui::MenuItem("Hex Viewer",        nullptr, &show_hex_);
        ImGui::MenuItem("Constructeur",      nullptr, &show_vendor_);
        ImGui::MenuItem("Deverrouillage",    nullptr, &show_unlock_);
        ImGui::MenuItem("Backup SA",         nullptr, &show_backup_);
        ImGui::MenuItem("Mots de passe SA",  nullptr, &show_pwd_);
        ImGui::Separator();
        ImGui::MenuItem("Console",           nullptr, &show_log_);
        ImGui::Separator();

        if (ImGui::BeginMenu("Theme")) {
            bool is_dark  = (theme_ == AppTheme::DARK);
            bool is_light = (theme_ == AppTheme::LIGHT);
            if (ImGui::MenuItem("Sombre", nullptr, &is_dark))
                apply_theme(AppTheme::DARK);
            if (ImGui::MenuItem("Clair",  nullptr, &is_light))
                apply_theme(AppTheme::LIGHT);
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Aide")) {
        if (ImGui::MenuItem("A propos..."))
            show_about_ = true;
        ImGui::EndMenu();
    }

    /* Indicateur droite */
    float w = ImGui::GetWindowWidth();
    char ind[64];
    snprintf(ind, sizeof(ind), "%zu disque(s)  |  %s",
             devices_.size(), selected_ >= 0 ? devices_[selected_].model : "---");
    float tw = ImGui::CalcTextSize(ind).x;
    ImGui::SameLine(w - tw - 16.0f);
    ImGui::TextDisabled("%s", ind);

    ImGui::EndMainMenuBar();
}

/* ==========================================================================
 *  Panel : Liste des disques (sidebar gauche)
 * ========================================================================== */

void GUI::draw_device_panel() {
    ImGui::Begin("Disques");

    /* Bouton scan */
    float avail = ImGui::GetContentRegionAvail().x;
    bool light = (theme_ == AppTheme::LIGHT);
    ImGui::PushStyleColor(ImGuiCol_Button,        light ? ImVec4(0.40f, 0.55f, 0.80f, 1.0f) : ImVec4(0.15f, 0.30f, 0.55f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  light ? ImVec4(0.34f, 0.50f, 0.76f, 1.0f) : ImVec4(0.20f, 0.40f, 0.70f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   light ? ImVec4(0.30f, 0.46f, 0.72f, 1.0f) : ImVec4(0.18f, 0.36f, 0.62f, 1.0f));
    if (ImGui::Button("Scanner (F5)", ImVec2(avail, 0)))
        do_scan();
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!scan_done_) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1),
            "Cliquez sur Scanner pour\ndecouvrir les disques.");
        ImGui::End();
        return;
    }

    if (devices_.empty()) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Aucun disque detecte.");
        ImGui::TextWrapped("Verifiez les droits Administrateur.");
        ImGui::End();
        return;
    }

    /* Liste des disques sous forme de cartes */
    for (int i = 0; i < static_cast<int>(devices_.size()); ++i) {
        const auto& d = devices_[i];
        bool is_sel = (selected_ == i);

        /* Couleur de fond selon selection */
        if (is_sel) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, light ? ImVec4(0.76f, 0.80f, 0.90f, 1.0f) : ImVec4(0.16f, 0.20f, 0.32f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, light ? ImVec4(0.90f, 0.90f, 0.93f, 1.0f) : ImVec4(0.11f, 0.11f, 0.15f, 1.0f));
        }

        char child_id[32];
        snprintf(child_id, sizeof(child_id), "##dev%d", i);
        ImGui::BeginChild(child_id, ImVec2(avail, 80), ImGuiChildFlags_Borders);

        /* Zone cliquable invisible */
        ImVec2 cpos = ImGui::GetCursorScreenPos();
        ImVec2 csize = ImGui::GetContentRegionAvail();
        if (ImGui::InvisibleButton(child_id, csize)) {
            selected_ = i;
            hex_valid_ = false;
            vendor_detected_ = false;
            unlock_state_ = UnlockState::IDLE;
            backup_done_ = false;
            passwords_extracted_ = false;
        }
        ImGui::SetCursorScreenPos(cpos);

        /* Indicateur securite */
        float rgb[3];
        security_color(d.security_status, rgb);
        ImGui::TextColored(ImVec4(rgb[0], rgb[1], rgb[2], 1),
                           "[%s]", security_label(d.security_status));
        ImGui::SameLine();
        ImGui::Text("Disque %d", i);

        /* Modele */
        ImGui::TextColored(ImVec4(0.75f, 0.82f, 0.95f, 1), "%s", d.model);

        /* Serial + Firmware */
        ImGui::TextDisabled("S/N: %s  |  FW: %s", d.serial, d.firmware);

        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::End();
}

/* ==========================================================================
 *  Panel : Details du disque
 * ========================================================================== */

void GUI::draw_details_panel() {
    ImGui::Begin("Details");

    if (selected_ < 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1),
            "Selectionnez un disque dans le panel de gauche.");
        ImGui::End();
        return;
    }

    const auto& d = devices_[selected_];

    /* En-tete */
    ImGui::TextColored(ImVec4(0.55f, 0.70f, 1.0f, 1), "%s", d.model);
    ImGui::Separator();
    ImGui::Spacing();

    /* Tableau d'infos */
    if (ImGui::BeginTable("##devinfo", 2, ImGuiTableFlags_None)) {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        auto row = [](const char* label, const char* fmt, auto... args) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%s", label);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(fmt, args...);
        };

        row("Chemin",    "%s", d.path);
        row("Modele",    "%s", d.model);
        row("N. Serie",  "%s", d.serial);
        row("Firmware",  "%s", d.firmware);

        /* Taille */
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Taille");
        ImGui::TableSetColumnIndex(1);
        if (d.size_sectors > 0) {
            double gb = static_cast<double>(d.size_sectors) * 512.0 / (1024.0*1024.0*1024.0);
            ImGui::Text("%.1f Go  (%llu secteurs)",
                        gb, static_cast<unsigned long long>(d.size_sectors));
        } else {
            ImGui::TextDisabled("N/A");
        }

        /* Security status brut */
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("Security (w128)");
        ImGui::TableSetColumnIndex(1);
        float rgb[3];
        security_color(d.security_status, rgb);
        ImGui::TextColored(ImVec4(rgb[0], rgb[1], rgb[2], 1),
                           "%s  (0x%04X)", security_label(d.security_status), d.security_status);

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /* Boutons d'action */
    if (ImGui::Button("Lire IDENTIFY brut"))
        do_identify_raw();
    ImGui::SameLine();
    if (ImGui::Button("Detecter constructeur"))
        do_detect_vendor();

    ImGui::End();
}

/* ==========================================================================
 *  Panel : Analyse de securite ATA
 * ========================================================================== */

void GUI::draw_security_panel() {
    ImGui::Begin("Securite ATA");

    if (selected_ < 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Selectionnez un disque.");
        ImGui::End();
        return;
    }

    const auto& d = devices_[selected_];
    uint16_t s = d.security_status;

    ImGui::TextColored(ImVec4(0.55f, 0.70f, 1.0f, 1), "%s", d.model);
    ImGui::Text("Word 128 brut : 0x%04X", s);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /* Flags individuels */
    struct FlagEntry { const char* name; uint16_t mask; const char* desc; };
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
        ImVec4 col = active
            ? ImVec4(0.30f, 0.90f, 0.40f, 1)
            : ImVec4(0.40f, 0.40f, 0.45f, 1);

        ImGui::TextColored(col, active ? "[X]" : "[ ]");
        ImGui::SameLine();
        ImGui::Text("%-18s", f.name);
        ImGui::SameLine();
        ImGui::TextDisabled("%s", f.desc);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /* Verdict */
    ImGui::Text("Verdict :");
    ImGui::SameLine();

    if (!(s & SEC_FLAG_SUPPORTED)) {
        ImGui::TextColored(ImVec4(0.3f,0.85f,0.3f,1),
            "Pas de securite ATA sur ce disque.");
    } else if (SEC_IS_EXPIRED(s)) {
        ImGui::TextColored(ImVec4(1,0.2f,0.2f,1),
            "BLOQUE — Compteur de tentatives expire. Power cycle requis.");
    } else if (SEC_IS_LOCKED(s) && SEC_IS_FROZEN(s)) {
        ImGui::TextColored(ImVec4(1,0.2f,0.2f,1),
            "VERROUILLE + GELE — Suspend-to-RAM ou hot-swap necessaire.");
    } else if (SEC_IS_LOCKED(s)) {
        ImGui::TextColored(ImVec4(1,0.3f,0.3f,1),
            "VERROUILLE — Password requis pour deverrouillage.");
    } else if (SEC_IS_FROZEN(s)) {
        ImGui::TextColored(ImVec4(1,0.8f,0,1),
            "GELE — Suspend-to-RAM pour degeler.");
    } else if (SEC_IS_ENABLED(s)) {
        ImGui::TextColored(ImVec4(0.8f,0.4f,0.8f,1),
            "PASSWORD ACTIF mais disque non verrouille.");
    } else {
        ImGui::TextColored(ImVec4(0.3f,0.85f,0.3f,1),
            "Securite supportee, aucun password actif.");
    }

    ImGui::End();
}

/* ==========================================================================
 *  Panel : Hex Viewer (IDENTIFY DEVICE brut)
 * ========================================================================== */

void GUI::draw_hex_viewer() {
    ImGui::Begin("Hex Viewer", &show_hex_);

    if (!hex_valid_) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1),
            "Appuyez sur 'Lire IDENTIFY brut' dans le panel Details.");
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("IDENTIFY DEVICE — 512 bytes (256 words)");
    ImGui::Spacing();

    /* Header */
    ImGui::Text("Offset  ");
    for (int c = 0; c < 16; ++c) {
        ImGui::SameLine(80.0f + c * 26.0f);
        ImGui::TextDisabled("%02X", c);
    }
    ImGui::SameLine(80.0f + 16 * 26.0f + 12.0f);
    ImGui::TextDisabled("ASCII");

    ImGui::Separator();

    /* Scrollable child */
    ImGui::BeginChild("##hexdata", ImVec2(0, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (int row = 0; row < 32; ++row) {
        int base = row * 16;

        /* Alternance de couleur de fond subtile */
        if (row % 2 == 1) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(
                p, ImVec2(p.x + 2000, p.y + ImGui::GetTextLineHeight()),
                IM_COL32(255, 255, 255, 8));
        }

        /* Offset */
        ImGui::TextColored(ImVec4(0.45f, 0.55f, 0.75f, 1), "%04X  ", base);

        /* Hex bytes */
        for (int col = 0; col < 16; ++col) {
            ImGui::SameLine(80.0f + col * 26.0f);
            uint8_t b = hex_data_[base + col];

            /* Surligner les words ATA importants */
            bool highlight = false;
            int word_idx = (base + col) / 2;
            if (word_idx == 128 || word_idx == 129)  /* security status + master pwd */
                highlight = true;

            if (highlight)
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1), "%02X", b);
            else
                ImGui::Text("%02X", b);
        }

        /* ASCII */
        ImGui::SameLine(80.0f + 16 * 26.0f + 12.0f);
        char ascii[17];
        for (int col = 0; col < 16; ++col) {
            uint8_t b = hex_data_[base + col];
            ascii[col] = (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';
        }
        ascii[16] = '\0';
        ImGui::TextDisabled("%s", ascii);
    }

    ImGui::EndChild();
    ImGui::End();
}

/* ==========================================================================
 *  Panel : Detection constructeur (VSC)
 * ========================================================================== */

void GUI::draw_vendor_panel() {
    ImGui::Begin("Constructeur", &show_vendor_);

    if (selected_ < 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Selectionnez un disque.");
        ImGui::End();
        return;
    }

    const auto& d = devices_[selected_];
    ImGui::Text("Disque : %s", d.model);
    ImGui::Spacing();

    float avail = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button("Detecter le constructeur", ImVec2(avail, 0)))
        do_detect_vendor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (vendor_detected_) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1), "Detecte :");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.55f, 0.70f, 1.0f, 1), "%s", vendor_name_.c_str());

        ImGui::Spacing();
        ImGui::TextWrapped("Les commandes VSC proprietaires sont disponibles. "
                           "Vous pouvez proceder au backup SA et au deverrouillage.");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1),
            "Cliquez sur le bouton ci-dessus pour analyser le modele "
            "et identifier le constructeur.");
        ImGui::Spacing();
        ImGui::TextDisabled("Constructeurs supportes :");
        ImGui::BulletText("Western Digital (VSC 0xE0/0xE1)");
        ImGui::BulletText("Seagate (SMART vendor 0xD6)");
        ImGui::BulletText("Toshiba (0xC0/0xC1)");
        ImGui::BulletText("HGST (0xC0/0xC1)");
    }

    ImGui::End();
}

/* ==========================================================================
 *  Panel : Deverrouillage
 * ========================================================================== */

void GUI::draw_unlock_panel() {
    ImGui::Begin("Deverrouillage", &show_unlock_);

    if (selected_ < 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Selectionnez un disque.");
        ImGui::End();
        return;
    }

    const auto& d = devices_[selected_];
    uint16_t s = d.security_status;

    /* Statut actuel */
    float rgb[3];
    security_color(s, rgb);
    ImGui::Text("Statut :");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(rgb[0], rgb[1], rgb[2], 1),
                       "%s", security_label(s));

    ImGui::Spacing();

    if (!SEC_IS_LOCKED(s) && unlock_state_ == UnlockState::IDLE) {
        ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.3f, 1),
            "Ce disque n'est pas verrouille.");
        ImGui::TextDisabled("Aucune action de deverrouillage necessaire.");
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::Spacing();

    /* Workflow explique */
    ImGui::TextWrapped("Sequence de deverrouillage :");
    ImGui::Spacing();

    ImGui::TextDisabled("1.");
    ImGui::SameLine();
    ImGui::Text("Detection constructeur (VSC)");

    ImGui::TextDisabled("2.");
    ImGui::SameLine();
    ImGui::Text("Backup Service Area");

    ImGui::TextDisabled("3.");
    ImGui::SameLine();
    ImGui::Text("RAM Patch (flag securite)");

    ImGui::TextDisabled("4.");
    ImGui::SameLine();
    ImGui::Text("SECURITY DISABLE PASSWORD");

    ImGui::TextDisabled("5.");
    ImGui::SameLine();
    ImGui::Text("Verification post-unlock");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    /* Bouton unlock */
    bool light_u = (theme_ == AppTheme::LIGHT);
    ImGui::PushStyleColor(ImGuiCol_Button,        light_u ? ImVec4(0.80f, 0.35f, 0.35f, 1) : ImVec4(0.55f, 0.15f, 0.15f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  light_u ? ImVec4(0.85f, 0.42f, 0.42f, 1) : ImVec4(0.70f, 0.20f, 0.20f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   light_u ? ImVec4(0.75f, 0.30f, 0.30f, 1) : ImVec4(0.62f, 0.18f, 0.18f, 1));

    float avail = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button("DEVERROUILLER", ImVec2(avail, 36)))
        do_unlock();

    ImGui::PopStyleColor(3);

    /* Resultat */
    ImGui::Spacing();

    if (unlock_state_ == UnlockState::SUCCESS) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, light_u ? ImVec4(0.80f, 0.95f, 0.80f, 1) : ImVec4(0.05f, 0.20f, 0.05f, 1));
        ImGui::BeginChild("##ok", ImVec2(avail, 50), ImGuiChildFlags_Borders);
        ImGui::TextColored(ImVec4(0.3f, 0.95f, 0.3f, 1), "SUCCES");
        ImGui::TextWrapped("%s", unlock_msg_.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    } else if (unlock_state_ == UnlockState::FAILED) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, light_u ? ImVec4(0.98f, 0.85f, 0.85f, 1) : ImVec4(0.20f, 0.05f, 0.05f, 1));
        ImGui::BeginChild("##fail", ImVec2(avail, 50), ImGuiChildFlags_Borders);
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "ECHEC");
        ImGui::TextWrapped("%s", unlock_msg_.c_str());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::End();
}

/* ==========================================================================
 *  Panel : Backup SA
 * ========================================================================== */

void GUI::draw_backup_panel() {
    ImGui::Begin("Backup SA", &show_backup_);

    if (selected_ < 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Selectionnez un disque.");
        ImGui::End();
        return;
    }

    const auto& d = devices_[selected_];
    ImGui::Text("Disque : %s", d.model);
    ImGui::Spacing();

    ImGui::TextWrapped("Sauvegarde les modules Service Area du disque "
                       "avant toute operation d'ecriture.");

    ImGui::Spacing();

    float avail = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button("Sauvegarder la SA", ImVec2(avail, 0)))
        do_backup_sa();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (backup_done_) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1), "Backup cree :");
        ImGui::TextWrapped("%s", backup_path_.c_str());

        if (!backup_sha_.empty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("SHA-256 :");
            ImGui::TextWrapped("%s", backup_sha_.c_str());
        }
    } else {
        ImGui::TextDisabled("Aucun backup effectue pour ce disque.");
    }

    ImGui::End();
}

/* ==========================================================================
 *  Panel : Mots de passe SA
 * ========================================================================== */

void GUI::draw_passwords_panel() {
    ImGui::Begin("Mots de passe SA", &show_pwd_);

    if (selected_ < 0) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "Selectionnez un disque.");
        ImGui::End();
        return;
    }

    const auto& d = devices_[selected_];
    ImGui::Text("Disque : %s", d.model);
    ImGui::Spacing();

    ImGui::TextWrapped("Lit le module password de la Service Area "
                       "pour extraire les mots de passe USER et MASTER.");

    ImGui::Spacing();

    float avail = ImGui::GetContentRegionAvail().x;
    if (ImGui::Button("Extraire les mots de passe", ImVec2(avail, 0)))
        do_extract_passwords();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!passwords_extracted_) {
        ImGui::TextDisabled("Aucune extraction effectuee.");
        ImGui::End();
        return;
    }

    if (!password_info_.found) {
        ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1),
            "Module lu — aucun password trouve.");
        ImGui::End();
        return;
    }

    /* Affichage des passwords */
    ImGui::Checkbox("Reveler les mots de passe", &reveal_passwords_);
    ImGui::Spacing();

    auto show_pwd = [&](const char* label, const std::string& pwd) {
        ImGui::TextDisabled("%s :", label);
        ImGui::SameLine();
        if (pwd.empty()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "(vide)");
        } else if (reveal_passwords_) {
            ImGui::TextColored(ImVec4(0.55f, 0.70f, 1.0f, 1), "%s", pwd.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1), "********");
        }
    };

    show_pwd("USER",   password_info_.user_password);
    show_pwd("MASTER", password_info_.master_password);

    if (password_info_.is_empty_password) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1, 0.8f, 0, 1),
            "Note : password vide (tous zeros)");
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Security flags : 0x%02X", password_info_.security_flags);

    ImGui::End();
}

/* ==========================================================================
 *  Panel : Console (journal)
 * ========================================================================== */

void GUI::draw_log_console() {
    ImGui::Begin("Console", &show_log_);

    /* Toolbar */
    if (ImGui::SmallButton("Effacer"))
        log_.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &log_autoscroll_);
    ImGui::SameLine();
    ImGui::TextDisabled("| %zu entrees", log_.size());

    ImGui::Separator();

    /* Messages */
    ImGui::BeginChild("##loglist", ImVec2(0, 0), ImGuiChildFlags_None);

    for (const auto& e : log_) {
        /* Timestamp */
        ImGui::TextDisabled("[%s]", e.timestamp.c_str());
        ImGui::SameLine();

        /* Level color */
        ImVec4 col;
        const char* prefix;
        switch (e.level) {
            case LogEntry::LVL_OK:   col = ImVec4(0.3f, 0.9f, 0.4f, 1); prefix = " OK  "; break;
            case LogEntry::LVL_WARN: col = ImVec4(1.0f, 0.8f, 0.2f, 1); prefix = "WARN "; break;
            case LogEntry::LVL_ERR:  col = ImVec4(1.0f, 0.3f, 0.3f, 1); prefix = " ERR "; break;
            default:                 col = ImVec4(0.6f, 0.7f, 0.8f, 1); prefix = "INFO "; break;
        }

        ImGui::TextColored(col, "%s", prefix);
        ImGui::SameLine();
        ImGui::TextWrapped("%s", e.message.c_str());
    }

    if (log_autoscroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}

/* ==========================================================================
 *  Status Bar (barre inferieure pleine largeur)
 * ========================================================================== */

void GUI::draw_status_bar() {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, (theme_ == AppTheme::LIGHT) ? ImVec4(0.90f, 0.90f, 0.92f, 1) : ImVec4(0.08f, 0.08f, 0.11f, 1));

    ImGui::Begin("##statusbar", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoDocking);

    if (status_timer_ > 0) {
        ImGui::TextUnformatted(status_msg_.c_str());
        status_timer_ -= ImGui::GetIO().DeltaTime;
    } else {
        ImGui::TextDisabled("Pret  |  %zu disque(s)  |  %s",
            devices_.size(),
            selected_ >= 0 ? devices_[selected_].model : "aucun selectionne");
    }

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

/* ==========================================================================
 *  Popup : A propos
 * ========================================================================== */

void GUI::draw_about_popup() {
    ImGui::OpenPopup("A propos");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440, 380));

    if (ImGui::BeginPopupModal("A propos", &show_about_,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {

        ImGui::Spacing();

        /* ---- Logo centre ---- */
        if (logo_srv_) {
            float disp_w = 80.0f;
            float disp_h = disp_w * ((float)logo_h_ / (float)logo_w_);
            float avail  = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX((avail - disp_w) * 0.5f + ImGui::GetCursorPosX());
            ImGui::Image((ImTextureID)logo_srv_, ImVec2(disp_w, disp_h));
            ImGui::Spacing();
        }

        ImGui::TextColored(ImVec4(0.55f, 0.70f, 1.0f, 1),
            "HDD Password Recovery Tool");
        ImGui::Text("Version 1.0.0");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Outil bas-niveau C++20 pour le diagnostic et la recuperation "
            "de mots de passe ATA sur disques durs verrouilles "
            "(HDD Security Feature Set).");
        ImGui::Spacing();
        ImGui::TextDisabled("Constructeurs supportes :");
        ImGui::BulletText("Western Digital (VSC 0xE0/0xE1)");
        ImGui::BulletText("Seagate (SMART vendor 0xD6)");
        ImGui::BulletText("Toshiba / HGST (0xC0/0xC1)");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Usage prive uniquement — outil de diagnostic.");

        ImGui::Spacing();
        float w = ImGui::GetContentRegionAvail().x;
        if (ImGui::Button("Fermer", ImVec2(w, 0)))
            show_about_ = false;

        ImGui::EndPopup();
    }
}
