/*
 * cli.cpp
 * --------------------------------------------------------------------------
 * Implementation de l'interface en ligne de commande.
 *
 * Fonctionnalites :
 *   - Scan automatique des disques au demarrage
 *   - Menu interactif (scan / select / info / security / quit)
 *   - Affichage tabulaire des disques detectes
 *   - Diagnostic detaille du statut de securite ATA
 * --------------------------------------------------------------------------
 */

#include "cli.h"
#include "config.h"
#include "vsc_engine.h"
#include "device.h"
#include "ram_patcher.h"
#include "sa_backup.h"
#include "sa_parser.h"
#include "logger.h"
#include "report.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winioctl.h>
#include <io.h>
#define IS_TTY _isatty(_fileno(stdout))
#else
#include <unistd.h>
#include <fcntl.h>
#define IS_TTY isatty(fileno(stdout))
#endif

static const char* GRN = "";
static const char* RED = "";
static const char* YEL = "";
static const char* CYN = "";
static const char* MAG = "";
static const char* BLD = "";
static const char* RST = "";

static void init_colors() {
    if (IS_TTY) {
        GRN = "\033[32m";
        RED = "\033[31m";
        YEL = "\033[33m";
        CYN = "\033[36m";
        MAG = "\033[35m";
        BLD = "\033[1m";
        RST = "\033[0m";
    }
}

/* ==========================================================================
 *  DeviceScanner
 * ========================================================================== */

std::vector<std::string> DeviceScanner::scan() {
    std::vector<std::string> found;

    constexpr int MAX_DRIVES = 16;

    for (int i = 0; i < MAX_DRIVES; ++i) {
        char path[DEVICE_PATH_MAX];
#ifdef _WIN32
        snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%d", i);

        /*
         * Sous Windows, on teste l'ouverture via CreateFileA directement.
         * Un disque LOCKED est toujours visible au niveau PhysicalDrive
         * car le verrouillage ATA empeche l'acces aux donnees, pas au handle.
         * On tente GENERIC_READ seul d'abord (suffisant pour IDENTIFY et IOCTLs).
         */
        HANDLE h = CreateFileA(
            path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );
        if (h != INVALID_HANDLE_VALUE) {
            found.emplace_back(path);
            CloseHandle(h);
        }
#else
        snprintf(path, sizeof(path), "/dev/sd%c", 'a' + i);
        int fd = ::open(path, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) {
            found.emplace_back(path);
            ::close(fd);
        }
#endif
    }
    return found;
}

bool DeviceScanner::probe_device(const std::string& path, DeviceInfo& info) {
    std::memset(&info, 0, sizeof(DeviceInfo));
    snprintf(info.path, DEVICE_PATH_MAX, "%s", path.c_str());

    /* Niveau 1 : ATA IDENTIFY DEVICE (0xEC) */
    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(path);
    if (err != ATAError::OK) {
        /*
         * Meme si notre ATAInterface ne peut pas ouvrir,
         * on tente le fallback OS (IOCTLs de stockage).
         */
        return probe_via_os_ioctl(path, info);
    }

    IdentifyData id{};
    err = h->identify_device(id);

    if (err == ATAError::OK) {
        /* IDENTIFY a reussi -- on a toutes les infos */
        ata_string_fixup(id.model_number, sizeof(id.model_number), info.model);
        ata_string_fixup(id.serial_number, sizeof(id.serial_number), info.serial);
        ata_string_fixup(id.firmware_rev, sizeof(id.firmware_rev), info.firmware);
        info.security_status = id.security_status;
        return true;
    }

    /*
     * Niveau 2 : IDENTIFY a echoue.
     * C'est typique d'un disque LOCKED ou d'un controleur USB qui
     * ne supporte pas ATA pass-through.
     * On utilise les IOCTLs OS pour recuperer le modele/serial.
     */
    if (probe_via_os_ioctl(path, info)) {
        /*
         * Le fallback a recupere des infos.
         * On marque SEC_FLAG_LOCKED comme probable si le modele
         * n'est pas vide (le disque repond aux IOCTLs de stockage
         * mais pas a IDENTIFY -- signe d'un disque locke).
         */
        if (info.security_status == 0)
            info.security_status = SEC_FLAG_SUPPORTED | SEC_FLAG_ENABLED | SEC_FLAG_LOCKED;
        return true;
    }

    /* Le disque est ouvert mais ni IDENTIFY ni fallback ne marchent */
    snprintf(info.model, MODEL_MAX, "(inconnu - pas de reponse ATA)");
    return true;
}

/* --------------------------------------------------------------------------
 *  probe_via_os_ioctl -- Fallback OS pour disques verouilles
 *
 *  Sous Windows : IOCTL_STORAGE_QUERY_PROPERTY retourne le modele et
 *  le serial meme sur un disque ATA-LOCKED car cette info vient du
 *  miniport driver, pas du firmware ATA.
 *
 *  Sous Linux : on lit /sys/block/sdX/device/model et serial.
 * -------------------------------------------------------------------------- */

#ifdef _WIN32

bool DeviceScanner::probe_via_os_ioctl(const std::string& path, DeviceInfo& info) {
    HANDLE h = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if (h == INVALID_HANDLE_VALUE) return false;

    /* --- IOCTL_STORAGE_QUERY_PROPERTY : modele + serial + firmware --- */

    struct {
        STORAGE_PROPERTY_QUERY query;
    } qbuf{};
    qbuf.query.PropertyId = StorageDeviceProperty;
    qbuf.query.QueryType  = PropertyStandardQuery;

    uint8_t out_buf[4096]{};
    DWORD bytes_ret = 0;

    BOOL ok = DeviceIoControl(
        h,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &qbuf, sizeof(qbuf),
        out_buf, sizeof(out_buf),
        &bytes_ret,
        nullptr
    );

    if (ok && bytes_ret >= sizeof(STORAGE_DEVICE_DESCRIPTOR)) {
        auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(out_buf);

        /* Modele */
        if (desc->ProductIdOffset && desc->ProductIdOffset < bytes_ret) {
            const char* prod = reinterpret_cast<const char*>(out_buf + desc->ProductIdOffset);
            snprintf(info.model, MODEL_MAX, "%s", prod);
            /* Trim trailing spaces */
            size_t len = strlen(info.model);
            while (len > 0 && info.model[len-1] == ' ') info.model[--len] = '\0';
        }

        /* Vendor (prefixe au modele si present) */
        if (desc->VendorIdOffset && desc->VendorIdOffset < bytes_ret) {
            const char* vendor = reinterpret_cast<const char*>(out_buf + desc->VendorIdOffset);
            if (vendor[0] && vendor[0] != ' ') {
                char combined[MODEL_MAX];
                snprintf(combined, MODEL_MAX, "%s %s", vendor, info.model);
                /* Trim trailing spaces */
                size_t len = strlen(combined);
                while (len > 0 && combined[len-1] == ' ') combined[--len] = '\0';
                snprintf(info.model, MODEL_MAX, "%s", combined);
            }
        }

        /* Serial */
        if (desc->SerialNumberOffset && desc->SerialNumberOffset < bytes_ret) {
            const char* serial = reinterpret_cast<const char*>(out_buf + desc->SerialNumberOffset);
            snprintf(info.serial, SERIAL_MAX, "%s", serial);
            size_t len = strlen(info.serial);
            while (len > 0 && info.serial[len-1] == ' ') info.serial[--len] = '\0';
        }

        /* Firmware */
        if (desc->ProductRevisionOffset && desc->ProductRevisionOffset < bytes_ret) {
            const char* fw = reinterpret_cast<const char*>(out_buf + desc->ProductRevisionOffset);
            snprintf(info.firmware, FIRMWARE_MAX, "%s", fw);
            size_t len = strlen(info.firmware);
            while (len > 0 && info.firmware[len-1] == ' ') info.firmware[--len] = '\0';
        }
    }

    /* --- IOCTL_DISK_GET_DRIVE_GEOMETRY : taille en secteurs --- */

    DISK_GEOMETRY geo{};
    ok = DeviceIoControl(
        h,
        IOCTL_DISK_GET_DRIVE_GEOMETRY,
        nullptr, 0,
        &geo, sizeof(geo),
        &bytes_ret,
        nullptr
    );
    if (ok) {
        info.size_sectors = static_cast<uint64_t>(
            geo.Cylinders.QuadPart * geo.TracksPerCylinder *
            geo.SectorsPerTrack);
    }

    CloseHandle(h);

    /* Succes si on a au moins un modele */
    return info.model[0] != '\0';
}

#else /* Linux */

bool DeviceScanner::probe_via_os_ioctl(const std::string& path, DeviceInfo& info) {
    /*
     * Pour /dev/sdX, on lit dans /sys/block/sdX/device/
     * les fichiers model, serial (rev pour firmware).
     */
    if (path.size() < 8) return false;  /* /dev/sdX minimum */

    /* Extraire "sdX" depuis /dev/sdX */
    std::string devname = path.substr(5);  /* "sda", "sdb", etc. */

    auto read_sysfs = [](const std::string& sysfs_path, char* out, size_t max) -> bool {
        FILE* f = fopen(sysfs_path.c_str(), "r");
        if (!f) return false;
        if (fgets(out, static_cast<int>(max), f)) {
            size_t len = strlen(out);
            while (len > 0 && (out[len-1] == '\n' || out[len-1] == ' '))
                out[--len] = '\0';
        }
        fclose(f);
        return out[0] != '\0';
    };

    std::string base = "/sys/block/" + devname + "/device/";
    read_sysfs(base + "model",  info.model,    MODEL_MAX);
    read_sysfs(base + "serial", info.serial,   SERIAL_MAX);
    read_sysfs(base + "rev",    info.firmware,  FIRMWARE_MAX);

    return info.model[0] != '\0';
}

#endif

/* ==========================================================================
 *  CLI - Affichage
 * ========================================================================== */

void CLI::print_banner() {
    printf("\n%s", YEL);
    printf("+=====================================================+\n");
    printf("|   HDD Password Recovery Tool  v1.0.0                |\n");
    printf("|   ATA Security Feature Set - Diagnostic & Unlock    |\n");
    printf("+=====================================================+\n");
    printf("%s\n", RST);
}

void CLI::print_help() {
    printf("\n%sCommandes disponibles :%s\n", BLD, RST);
    printf("  %sscan%s        Detecter les disques physiques\n", CYN, RST);
    printf("  %ssel <N>%s     Selectionner le disque N\n", CYN, RST);
    printf("  %sinfo%s        Afficher les details du disque selectionne\n", CYN, RST);
    printf("  %ssecurity%s    Analyser le statut de securite ATA\n", CYN, RST);
    printf("  %sraw%s         Afficher IDENTIFY DEVICE brut (hex)\n", CYN, RST);
    printf("  %sdetect%s      Detecter le constructeur (VSC engine)\n", CYN, RST);
    printf("  %sunlock%s      Deverrouiller le disque (RAM patch / password)\n", CYN, RST);
    printf("  %sbackup%s      Sauvegarder la Service Area\n", CYN, RST);
    printf("  %spasswords%s   Extraire les passwords de la SA\n", CYN, RST);
    printf("  %shelp%s        Afficher cette aide\n", CYN, RST);
    printf("  %squit%s        Quitter\n", CYN, RST);
    printf("\n");
}

void CLI::print_device_table(const std::vector<DeviceInfo>& devices) {
    if (devices.empty()) {
        printf("\n  %sAucun disque detecte.%s\n", RED, RST);
        printf("  Verifiez que vous executez en tant qu'Administrateur/root.\n\n");
        return;
    }

    printf("\n  %s%-4s %-40s %-20s %-8s %-10s%s\n",
           BLD, "#", "Modele", "Serial", "FW", "Securite", RST);
    printf("  ---- ----------------------------------------"
           " -------------------- --------"
           " ----------\n");

    for (size_t i = 0; i < devices.size(); ++i) {
        const auto& d = devices[i];

        const char* sec_color = RST;
        const char* sec_label = "---";

        if (SEC_IS_LOCKED(d.security_status)) {
            sec_color = RED;
            sec_label = "LOCKED";
        } else if (SEC_IS_FROZEN(d.security_status)) {
            sec_color = YEL;
            sec_label = "FROZEN";
        } else if (SEC_IS_ENABLED(d.security_status)) {
            sec_color = MAG;
            sec_label = "ENABLED";
        } else if (d.security_status & SEC_FLAG_SUPPORTED) {
            sec_color = GRN;
            sec_label = "OK";
        }

        /* Indicateur si invisible dans l'explorateur */
        const char* vis = "";
        if (SEC_IS_LOCKED(d.security_status))
            vis = " (!)";

        printf("  [%s%2zu%s] %-40s %-20s %-8s %s%-10s%s%s\n",
               CYN, i, RST,
               d.model, d.serial, d.firmware,
               sec_color, sec_label, RST, vis);
    }

    /* Legende si au moins un disque est locke */
    bool has_locked = false;
    for (const auto& d : devices)
        if (SEC_IS_LOCKED(d.security_status)) { has_locked = true; break; }
    if (has_locked) {
        printf("  %s(!) = Disque verrouille (invisible dans l'Explorateur Windows)%s\n", RED, RST);
        printf("      Info obtenue via IOCTL de stockage (pas ATA IDENTIFY).\n");
    }
    printf("\n");
}

void CLI::print_device_detail(const DeviceInfo& info) {
    printf("\n  %s+-- Informations du disque --%s\n", BLD, RST);
    printf("  | Chemin     : %s%s%s\n", CYN, info.path, RST);
    printf("  | Modele     : %s\n", info.model);
    printf("  | Serial     : %s\n", info.serial);
    printf("  | Firmware   : %s\n", info.firmware);

    char sec_str[64];
    ata_security_status_str(info.security_status, sec_str);
    printf("  | Securite   : %s (word 128 = 0x%04X)\n", sec_str, info.security_status);
    printf("  +----------------------------\n\n");
}

void CLI::print_security_analysis(const DeviceInfo& info) {
    uint16_t s = info.security_status;

    printf("\n  %s+== Analyse de securite ATA ==%s\n", BLD, RST);
    printf("  | Disque : %s (%s)\n", info.model, info.path);
    printf("  | Word 128 brut : 0x%04X\n", s);
    printf("  |\n");

    /* Flags individuels */
    auto flag = [&](const char* name, bool active, const char* desc) {
        printf("  |   [%s%s%s] %-20s %s\n",
               active ? GRN : RED,
               active ? "X" : " ",
               RST, name, desc);
    };

    flag("SUPPORTED",      s & SEC_FLAG_SUPPORTED,      "Security Feature Set disponible");
    flag("ENABLED",        s & SEC_FLAG_ENABLED,        "Un password est defini");
    flag("LOCKED",         s & SEC_FLAG_LOCKED,         "Disque verrouille - acces bloque");
    flag("FROZEN",         s & SEC_FLAG_FROZEN,         "Securite gelee par BIOS/OS");
    flag("COUNT_EXPIRED",  s & SEC_FLAG_COUNT_EXPIRED,  "Trop de tentatives echouees");
    flag("ENHANCED_ERASE", s & SEC_FLAG_ENHANCED_ERASE, "Enhanced Secure Erase dispo");

    printf("  |\n");

    /* Verdict */
    std::string verdict = security_verdict(s);
    printf("  | %sVerdict%s : %s\n", BLD, RST, verdict.c_str());
    printf("  +==============================\n\n");
}

std::string CLI::security_verdict(uint16_t s) {
    if (!(s & SEC_FLAG_SUPPORTED))
        return std::string(GRN) + "Pas de securite ATA sur ce disque." + RST;

    if (SEC_IS_EXPIRED(s))
        return std::string(RED) + "BLOQUE - Compteur de tentatives expire. "
               "Power cycle requis." + RST;

    if (SEC_IS_LOCKED(s) && SEC_IS_FROZEN(s))
        return std::string(RED) + "VERROUILLE + GELE - Suspend-to-RAM ou "
               "hot-swap necessaire avant deverrouillage." + RST;

    if (SEC_IS_LOCKED(s))
        return std::string(RED) + "VERROUILLE - Password requis. "
               "Utilisez 'unlock' (sprint futur)." + RST;

    if (SEC_IS_FROZEN(s))
        return std::string(YEL) + "GELE - L'OS/BIOS a gele la securite. "
               "Suspend-to-RAM pour degeler." + RST;

    if (SEC_IS_ENABLED(s))
        return std::string(MAG) + "PASSWORD ACTIF mais disque non verrouille. "
               "'disable' possible." + RST;

    return std::string(GRN) + "Securite supportee, aucun password actif." + RST;
}

/* ==========================================================================
 *  CLI - Actions
 * ========================================================================== */

void CLI::cmd_scan() {
    printf("\n  Scan des disques en cours...\n");

    auto paths = DeviceScanner::scan();
    devices_.clear();
    selected_ = -1;

    for (const auto& p : paths) {
        DeviceInfo info{};
        if (DeviceScanner::probe_device(p, info)) {
            devices_.push_back(info);
        }
    }

    printf("  %s%zu disque(s) detecte(s).%s\n", GRN, devices_.size(), RST);
    print_device_table(devices_);
}

void CLI::cmd_select(int index) {
    if (index < 0 || index >= static_cast<int>(devices_.size())) {
        printf("  %sIndex invalide. Utilisez 'scan' d'abord.%s\n", RED, RST);
        return;
    }
    selected_ = index;
    printf("  Disque selectionne : %s[%d]%s %s\n",
           CYN, selected_, RST, devices_[selected_].model);
}

void CLI::cmd_info() {
    if (selected_ < 0) {
        printf("  %sAucun disque selectionne. Utilisez 'sel <N>'.%s\n", RED, RST);
        return;
    }
    print_device_detail(devices_[selected_]);
}

void CLI::cmd_security() {
    if (selected_ < 0) {
        printf("  %sAucun disque selectionne. Utilisez 'sel <N>'.%s\n", RED, RST);
        return;
    }
    print_security_analysis(devices_[selected_]);
}

void CLI::cmd_identify_raw() {
    if (selected_ < 0) {
        printf("  %sAucun disque selectionne. Utilisez 'sel <N>'.%s\n", RED, RST);
        return;
    }

    const auto& dev = devices_[selected_];

    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(dev.path);
    if (err != ATAError::OK) {
        printf("  %sErreur ouverture : %s%s\n", RED, ata_error_str(err), RST);
        return;
    }

    IdentifyData id{};
    err = h->identify_device(id);
    if (err != ATAError::OK) {
        printf("  %sErreur IDENTIFY : %s%s\n", RED, ata_error_str(err), RST);
        if (SEC_IS_LOCKED(dev.security_status)) {
            printf("  %sLe disque est verrouille (LOCKED) -- IDENTIFY DEVICE echoue.%s\n",
                   YEL, RST);
            printf("  Les infos affichees par 'info' proviennent du driver de stockage.\n");
            printf("  Pour obtenir le dump complet, deverrouillez d'abord le disque.\n\n");
        }
        return;
    }

    const auto* raw = reinterpret_cast<const uint8_t*>(&id);

    printf("\n  %sIDENTIFY DEVICE - 512 bytes bruts :%s\n\n", BLD, RST);
    printf("  Offset  ");
    for (int c = 0; c < 16; ++c) printf(" %02X", c);
    printf("   ASCII\n");
    printf("  ------  -----------------------------------------------   ----------------\n");

    for (int row = 0; row < 32; ++row) {
        printf("  %04X    ", row * 16);
        for (int col = 0; col < 16; ++col) {
            int off = row * 16 + col;
            printf("%02X ", raw[off]);
        }
        printf("  ");
        for (int col = 0; col < 16; ++col) {
            int off = row * 16 + col;
            char c = static_cast<char>(raw[off]);
            printf("%c", (c >= 0x20 && c < 0x7F) ? c : '.');
        }
        printf("\n");
    }
    printf("\n");
}

/* ==========================================================================
 *  CLI - Nouvelles commandes F05/F10/F13/F14
 * ========================================================================== */

void CLI::cmd_detect() {
    if (selected_ < 0) {
        printf("  %sAucun disque selectionne. Utilisez 'sel <N>'.%s\n", RED, RST);
        return;
    }

    const auto& di = devices_[selected_];

    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(di.path);
    if (err != ATAError::OK) {
        printf("  %sErreur ouverture : %s%s\n", RED, ata_error_str(err), RST);
        return;
    }

    IdentifyData id{};
    err = h->identify_device(id);
    if (err != ATAError::OK) {
        printf("  %sErreur IDENTIFY : %s (disque peut-etre LOCKED)%s\n",
               RED, ata_error_str(err), RST);
        return;
    }

    VSCEngine engine(h.get());
    auto handler = engine.detect_vendor(id);

    if (handler) {
        printf("  %sConstructeur detecte : %s%s%s\n",
               GRN, BLD, handler->vendor_name(), RST);
    } else {
        printf("  %sConstructeur inconnu — VSC non disponible.%s\n", YEL, RST);
    }
}

void CLI::cmd_unlock() {
    if (selected_ < 0) {
        printf("  %sAucun disque selectionne. Utilisez 'sel <N>'.%s\n", RED, RST);
        return;
    }

    const auto& di = devices_[selected_];

    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(di.path);
    if (err != ATAError::OK) {
        printf("  %sErreur ouverture : %s%s\n", RED, ata_error_str(err), RST);
        return;
    }

    Device dev(h.get());
    if (!dev.refresh_identify()) {
        printf("  %sImpossible de lire IDENTIFY (disque LOCKED).%s\n", YEL, RST);
        /* Charger ce qu'on a depuis le scan */
        IdentifyData fallback{};
        memcpy(fallback.model_number, di.model, sizeof(fallback.model_number));
        memcpy(fallback.serial_number, di.serial, sizeof(fallback.serial_number));
        fallback.security_status = di.security_status;
        dev.set_identify(fallback);
    }

    Report report;
    report.set_device(di);
    report.set_initial_status(di.security_status);
    report.start_timer();

    printf("\n  %s=== Tentative de deverrouillage ===%s\n", BLD, RST);
    printf("  Disque : %s (%s)\n", di.model, di.path);

    if (Config::get().dry_run)
        printf("  %s[MODE DRY-RUN — aucune ecriture ne sera effectuee]%s\n", YEL, RST);

    VSCEngine engine(h.get());
    RAMPatcher patcher(h.get(), engine);

    UnlockResult result = patcher.attempt_unlock(dev);

    report.stop_timer();
    report.set_result(result);
    report.set_method("RAM Patch + Password Fallback");

    if (!patcher.last_backup_path().empty()) {
        report.set_backup_path(patcher.last_backup_path());
        auto sha = SABackup::sha256_file(patcher.last_backup_path());
        report.set_backup_sha256(sha);
    }

    /* Affichage du resultat */
    const char* color = (result == UnlockResult::SUCCESS ||
                         result == UnlockResult::PASSWORD_UNLOCK) ? GRN : RED;
    if (result == UnlockResult::DRY_RUN) color = YEL;

    printf("\n  %sResultat : %s%s\n\n", color, unlock_result_str(result), RST);

    /* Generation du rapport */
    std::string rpt_path = report.generate();
    if (!rpt_path.empty())
        printf("  Rapport genere : %s\n\n", rpt_path.c_str());

    /* Rafraichir le cache si unlock reussi */
    if (result == UnlockResult::SUCCESS || result == UnlockResult::PASSWORD_UNLOCK) {
        DeviceInfo refreshed{};
        if (DeviceScanner::probe_device(di.path, refreshed))
            devices_[selected_] = refreshed;
    }
}

void CLI::cmd_backup() {
    if (selected_ < 0) {
        printf("  %sAucun disque selectionne. Utilisez 'sel <N>'.%s\n", RED, RST);
        return;
    }

    const auto& di = devices_[selected_];

    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(di.path);
    if (err != ATAError::OK) {
        printf("  %sErreur ouverture : %s%s\n", RED, ata_error_str(err), RST);
        return;
    }

    IdentifyData id{};
    err = h->identify_device(id);
    if (err != ATAError::OK) {
        printf("  %sErreur IDENTIFY : %s%s\n", RED, ata_error_str(err), RST);
        return;
    }

    VSCEngine engine(h.get());
    auto handler = engine.detect_vendor(id);
    if (!handler) {
        printf("  %sConstructeur non reconnu — backup VSC impossible.%s\n", RED, RST);
        return;
    }

    printf("  Constructeur : %s\n", handler->vendor_name());
    printf("  Backup Service Area en cours...\n");

    std::string path = SABackup::backup_sa(handler.get(), di.serial, di.model);

    if (path.empty()) {
        printf("  %sEchec du backup SA.%s\n", RED, RST);
    } else {
        auto sha = SABackup::sha256_file(path);
        printf("  %sBackup cree : %s%s\n", GRN, path.c_str(), RST);
        printf("  SHA-256    : %s\n\n", sha.c_str());
    }
}

void CLI::cmd_passwords() {
    if (selected_ < 0) {
        printf("  %sAucun disque selectionne. Utilisez 'sel <N>'.%s\n", RED, RST);
        return;
    }

    const auto& di = devices_[selected_];

    ATAHandle h(ATAInterface::create());
    ATAError err = h.open(di.path);
    if (err != ATAError::OK) {
        printf("  %sErreur ouverture : %s%s\n", RED, ata_error_str(err), RST);
        return;
    }

    IdentifyData id{};
    err = h->identify_device(id);
    if (err != ATAError::OK) {
        printf("  %sErreur IDENTIFY : %s%s\n", RED, ata_error_str(err), RST);
        return;
    }

    VSCEngine engine(h.get());
    auto handler = engine.detect_vendor(id);
    if (!handler) {
        printf("  %sConstructeur non reconnu — lecture SA impossible.%s\n", RED, RST);
        return;
    }

    printf("  Constructeur : %s\n", handler->vendor_name());
    printf("  Lecture du module password SA (0x%02X)...\n", WD_SA_MODULE_PASSWORD);

    if (!handler->enter_vendor_mode()) {
        printf("  %sEchec entree mode vendor.%s\n", RED, RST);
        return;
    }
    VendorModeGuard guard(handler.get());

    uint8_t sa_buf[512]{};
    std::span<uint8_t> buf_span(sa_buf, sizeof(sa_buf));
    if (!handler->read_sa_module(WD_SA_MODULE_PASSWORD, buf_span)) {
        printf("  %sEchec lecture module 0x%02X.%s\n", RED, WD_SA_MODULE_PASSWORD, RST);
        return;
    }

    auto info = SAParser::parse_wd_password_module(sa_buf, sizeof(sa_buf));

    if (!info.found) {
        printf("  %sAucun password trouve dans le module SA.%s\n", YEL, RST);
        return;
    }

    bool show = Config::get().show_pwd;

    printf("\n  %s+-- Passwords extraits de la SA --%s\n", BLD, RST);
    printf("  | User   : %s\n",
           SAParser::mask_password(info.user_password, show).c_str());
    printf("  | Master : %s\n",
           SAParser::mask_password(info.master_password, show).c_str());
    printf("  | Flags  : 0x%02X\n", info.security_flags);
    if (info.is_empty_password)
        printf("  | %sATTENTION : passwords vides (0x00)%s\n", YEL, RST);
    if (!show)
        printf("  | (Utilisez --show-password pour afficher en clair)\n");
    printf("  +--------------------------------\n\n");
}

/* ==========================================================================
 *  CLI - Boucle principale
 * ========================================================================== */

int CLI::run(int argc, char* argv[]) {
    init_colors();

    /* Initialiser le logger */
    Logger::init(Config::get().verbose, Config::get().debug, Config::get().quiet);

    print_banner();

    /* Scan automatique au demarrage */
    cmd_scan();

    if (!devices_.empty()) {
        cmd_select(0);
    }

    print_help();

    /* Boucle interactive */
    char line[256];
    for (;;) {
        /* Prompt */
        if (selected_ >= 0)
            printf("%s[%d:%s]%s> ", CYN, selected_, devices_[selected_].model, RST);
        else
            printf("%shdd%s> ", CYN, RST);

        if (!fgets(line, sizeof(line), stdin))
            break;

        /* Trim newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* Parse commande */
        if (len == 0) continue;

        char cmd[32] = {};
        int arg = -1;
        sscanf(line, "%31s %d", cmd, &arg);

        std::string c(cmd);

        if (c == "quit" || c == "exit" || c == "q") {
            printf("\n  Au revoir.\n\n");
            break;
        }
        else if (c == "scan")       cmd_scan();
        else if (c == "sel")        cmd_select(arg);
        else if (c == "info")       cmd_info();
        else if (c == "security")   cmd_security();
        else if (c == "raw")        cmd_identify_raw();
        else if (c == "detect")     cmd_detect();
        else if (c == "unlock")     cmd_unlock();
        else if (c == "backup")     cmd_backup();
        else if (c == "passwords")  cmd_passwords();
        else if (c == "help" || c == "?") print_help();
        else {
            printf("  %sCommande inconnue : '%s'. Tapez 'help'.%s\n", RED, cmd, RST);
        }
    }

    return 0;
}
