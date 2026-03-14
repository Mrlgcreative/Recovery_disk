/*
 * report.cpp — Rapport d'intervention texte (F16)
 */

#include "report.h"
#include "sa_backup.h"

#include <cstdio>
#include <ctime>
#include <cstring>

/* ==========================================================================
 *  Report::generate
 * ========================================================================== */

std::string Report::generate() const {
    /* Nom du fichier : hdd_report_<serial>_<date>.txt */
    char filename[128];
    time_t now = time(nullptr);
    struct tm t;
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif

    char serial_clean[22]{};
    snprintf(serial_clean, sizeof(serial_clean), "%s", device_.serial);
    /* Remplacer les espaces par des underscores dans le serial */
    for (int i = 0; serial_clean[i]; ++i)
        if (serial_clean[i] == ' ') serial_clean[i] = '_';

    snprintf(filename, sizeof(filename),
             "hdd_report_%s_%04d%02d%02d_%02d%02d%02d.txt",
             serial_clean,
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);

    FILE* f = fopen(filename, "w");
    if (!f) return "";

    /* En-tete */
    fprintf(f, "================================================================\n");
    fprintf(f, "  RAPPORT D'INTERVENTION - HDD Password Recovery Tool\n");
    fprintf(f, "================================================================\n\n");

    fprintf(f, "Date     : %04d-%02d-%02d %02d:%02d:%02d\n",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);

    /* Duree */
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_ - start_);
    fprintf(f, "Duree    : %lld secondes\n\n", static_cast<long long>(duration.count()));

    /* Identification du disque */
    fprintf(f, "--- Disque ---\n");
    fprintf(f, "  Chemin   : %s\n", device_.path);
    fprintf(f, "  Modele   : %s\n", device_.model);
    fprintf(f, "  Serial   : %s\n", device_.serial);
    fprintf(f, "  Firmware : %s\n", device_.firmware);

    /* Etat initial */
    char sec_str[64];
    ata_security_status_str(initial_status_, sec_str);
    fprintf(f, "\n--- Etat initial ---\n");
    fprintf(f, "  Security Status : 0x%04X (%s)\n", initial_status_, sec_str);

    /* Methode */
    fprintf(f, "\n--- Methode ---\n");
    fprintf(f, "  %s\n", method_.c_str());

    /* Commandes envoyees */
    if (!commands_.empty()) {
        fprintf(f, "\n--- Commandes ATA envoyees ---\n");
        for (const auto& cmd : commands_) {
            fprintf(f, "  %s\n", cmd.c_str());
        }
    }

    /* Resultat */
    fprintf(f, "\n--- Resultat ---\n");
    fprintf(f, "  %s\n", unlock_result_str(result_));

    /* Backup */
    if (!backup_path_.empty()) {
        fprintf(f, "\n--- Backup SA ---\n");
        fprintf(f, "  Fichier : %s\n", backup_path_.c_str());
        if (!backup_sha256_.empty())
            fprintf(f, "  SHA-256 : %s\n", backup_sha256_.c_str());
    }

    fprintf(f, "\n================================================================\n");
    fprintf(f, "  Fin du rapport\n");
    fprintf(f, "================================================================\n");

    fclose(f);
    return filename;
}
