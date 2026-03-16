#pragma once
/*
 * sa_backup.h — Backup systématique des modules SA avant écriture (F13)
 */

#include "vendor_handler.h"

#include <cstdint>
#include <string>
#include <vector>

/* ==========================================================================
 *  SABackup — Dump et restauration de la Service Area
 * ========================================================================== */

namespace SABackup {

    /* Header du fichier de backup */
    #pragma pack(push, 1)
    struct BackupHeader {
        char     magic[8];          /* "HDDBACK\0"                     */
        uint32_t version;           /* 1                                */
        char     vendor[32];        /* Nom du constructeur              */
        char     serial[22];        /* Serial du disque                 */
        char     model[42];         /* Modele du disque                 */
        uint32_t module_count;      /* Nombre de modules sauvegardes    */
        uint32_t timestamp;         /* Unix timestamp                   */
        uint8_t  _pad[396];        /* Reserve — total header = 512     */
    };
    #pragma pack(pop)

    static_assert(sizeof(BackupHeader) == 512, "BackupHeader doit faire 512 bytes");

    /* Module individuel dans le backup */
    struct BackupModule {
        uint8_t module_id;
        std::vector<uint8_t> data;
    };

    /*
     * backup_sa — Sauvegarde tous les modules SA accessibles.
     * Retourne le chemin du fichier cree, ou "" en cas d'erreur.
     * Format : hdd_backup_<serial>_<YYYYMMDD_HHMMSS>.bin
     */
    std::string backup_sa(VendorHandler* handler,
                          const char* serial, const char* model);

    /*
     * restore_sa — Restaure les modules SA depuis un fichier backup.
     * Retourne true si succes.
     */
    bool restore_sa(VendorHandler* handler, const std::string& backup_path);

    /* Calcule le SHA-256 d'un fichier (retourne la string hex) */
    std::string sha256_file(const std::string& path);

    /* Genere le nom de fichier de backup avec horodatage */
    std::string make_backup_filename(const char* serial);
}
