#pragma once
/*
 * report.h — Rapport d'intervention texte (F16)
 */

#include "ata_defs.h"
#include "ram_patcher.h"

#include <string>
#include <vector>
#include <chrono>

/* ==========================================================================
 *  Report — Generation du rapport d'intervention
 * ========================================================================== */

class Report {
public:
    void set_device(const DeviceInfo& info) { device_ = info; }
    void set_initial_status(uint16_t status) { initial_status_ = status; }
    void set_method(const std::string& method) { method_ = method; }
    void set_result(UnlockResult result) { result_ = result; }
    void set_backup_path(const std::string& path) { backup_path_ = path; }
    void set_backup_sha256(const std::string& sha) { backup_sha256_ = sha; }

    void add_command_log(const std::string& entry) { commands_.push_back(entry); }

    void start_timer() { start_ = std::chrono::steady_clock::now(); }
    void stop_timer()  { end_ = std::chrono::steady_clock::now(); }

    /*
     * generate — Ecrit le rapport dans hdd_report_<serial>_<date>.txt
     * Retourne le chemin du fichier cree, ou "" en cas d'erreur.
     */
    std::string generate() const;

private:
    DeviceInfo  device_{};
    uint16_t    initial_status_ = 0;
    std::string method_;
    UnlockResult result_ = UnlockResult::VENDOR_NOT_SUPPORTED;
    std::string backup_path_;
    std::string backup_sha256_;
    std::vector<std::string> commands_;
    std::chrono::steady_clock::time_point start_;
    std::chrono::steady_clock::time_point end_;
};
