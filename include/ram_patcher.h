#pragma once
/*
 * ram_patcher.h — Algorithme de deverrouillage complet (F10)
 */

#include "vsc_engine.h"
#include "device.h"
#include "sa_parser.h"

#include <string>

/* ==========================================================================
 *  UnlockResult — Resultat de la tentative de deverrouillage
 * ========================================================================== */

enum class UnlockResult {
    SUCCESS,                /* Deverrouillage reussi                         */
    VENDOR_NOT_SUPPORTED,   /* Constructeur non reconnu                      */
    VENDOR_MODE_FAIL,       /* Impossible d'entrer en mode vendor            */
    RAM_PATCH_FAIL,         /* Echec du patch RAM                            */
    ATA_CMD_FAIL,           /* Echec de la commande ATA SECURITY DISABLE     */
    STILL_LOCKED,           /* Disque toujours verrouille apres tentative    */
    FROZEN,                 /* Disque gele — impossible de continuer         */
    EXPIRED,                /* Compteur de tentatives expire                 */
    BACKUP_FAIL,            /* Echec du backup SA                            */
    PASSWORD_UNLOCK,        /* Deverrouille via password SA (pas RAM patch)  */
    DRY_RUN,                /* Mode simulation — pas d'ecriture effective     */
};

const char* unlock_result_str(UnlockResult r);

/* ==========================================================================
 *  RAMPatcher — Orchestration complete du deverrouillage
 * ========================================================================== */

class RAMPatcher {
public:
    RAMPatcher(ATAInterface* ata, VSCEngine& engine)
        : ata_(ata), engine_(engine) {}

    /*
     * attempt_unlock — Tentative complete de deverrouillage.
     *
     * Sequence :
     *   1. detect_vendor()
     *   2. Backup SA (sauf --force)
     *   3. enter_vendor_mode() + VendorModeGuard RAII
     *   4. patch_security_ram()
     *   5. SECURITY DISABLE PASSWORD (0xF6) avec buffer zero
     *   6. refresh IDENTIFY → verification is_locked() == false
     *
     * Fallback si patch echoue :
     *   7. Lire passwords SA
     *   8. SECURITY UNLOCK (0xF2) + SECURITY DISABLE (0xF6)
     */
    UnlockResult attempt_unlock(Device& dev);

    /* Derniere info de password extraite (si applicable) */
    const PasswordInfo& last_password_info() const { return pwd_info_; }

    /* Chemin du dernier backup SA cree */
    const std::string& last_backup_path() const { return backup_path_; }

private:
    ATAInterface* ata_;
    VSCEngine&    engine_;
    PasswordInfo  pwd_info_;
    std::string   backup_path_;

    /* Sous-routines */
    bool send_security_disable(const uint8_t* password = nullptr);
    bool send_security_unlock(const uint8_t* password);
};
