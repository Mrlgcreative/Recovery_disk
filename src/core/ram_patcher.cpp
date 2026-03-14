/*
 * ram_patcher.cpp — Algorithme de deverrouillage complet (F10)
 */

#include "ram_patcher.h"
#include "sa_backup.h"
#include "config.h"

#include <cstdio>
#include <cstring>

/* ==========================================================================
 *  unlock_result_str
 * ========================================================================== */

const char* unlock_result_str(UnlockResult r) {
    switch (r) {
        case UnlockResult::SUCCESS:              return "SUCCES - Disque deverrouille";
        case UnlockResult::VENDOR_NOT_SUPPORTED: return "Constructeur non supporte";
        case UnlockResult::VENDOR_MODE_FAIL:     return "Echec entree mode vendor";
        case UnlockResult::RAM_PATCH_FAIL:       return "Echec patch RAM";
        case UnlockResult::ATA_CMD_FAIL:         return "Echec commande ATA";
        case UnlockResult::STILL_LOCKED:         return "Disque toujours verrouille";
        case UnlockResult::FROZEN:               return "Disque gele (FROZEN)";
        case UnlockResult::EXPIRED:              return "Compteur de tentatives expire";
        case UnlockResult::BACKUP_FAIL:          return "Echec backup SA";
        case UnlockResult::PASSWORD_UNLOCK:      return "Deverrouille via password SA";
        case UnlockResult::DRY_RUN:              return "Mode simulation (dry-run)";
        default:                                 return "Resultat inconnu";
    }
}

/* ==========================================================================
 *  Sous-routines ATA standard
 * ========================================================================== */

bool RAMPatcher::send_security_disable(const uint8_t* password) {
    uint8_t pwd_buf[512]{};
    if (password)
        memcpy(pwd_buf + 2, password, 32);   /* offset 2 = password data */

    ATACommand cmd{};
    cmd.command   = ATA_CMD_SECURITY_DISABLE;   /* 0xF6 */
    cmd.buffer    = pwd_buf;
    cmd.data_size = 512;
    cmd.write     = 1;

    if (Config::get().dry_run) {
        printf("  [DRY-RUN] SECURITY DISABLE PASSWORD (0xF6) - simule\n");
        return true;
    }

    return ata_->send_command(cmd) == ATAError::OK;
}

bool RAMPatcher::send_security_unlock(const uint8_t* password) {
    uint8_t pwd_buf[512]{};
    if (password)
        memcpy(pwd_buf + 2, password, 32);   /* offset 2 = password data */

    ATACommand cmd{};
    cmd.command   = ATA_CMD_SECURITY_UNLOCK;    /* 0xF2 */
    cmd.buffer    = pwd_buf;
    cmd.data_size = 512;
    cmd.write     = 1;

    if (Config::get().dry_run) {
        printf("  [DRY-RUN] SECURITY UNLOCK (0xF2) - simule\n");
        return true;
    }

    return ata_->send_command(cmd) == ATAError::OK;
}

/* ==========================================================================
 *  attempt_unlock — Sequence complete
 * ========================================================================== */

UnlockResult RAMPatcher::attempt_unlock(Device& dev) {
    /* Preconditions */
    if (dev.is_expired())
        return UnlockResult::EXPIRED;

    if (dev.is_frozen()) {
        if (!dev.attempt_unfreeze())
            return UnlockResult::FROZEN;
    }

    /* 1. Detection constructeur */
    auto handler = engine_.detect_vendor(dev.identify());
    if (!handler) return UnlockResult::VENDOR_NOT_SUPPORTED;

    printf("  Constructeur detecte : %s\n", handler->vendor_name());

    /* 2. Backup SA (sauf --force) */
    if (!Config::get().force) {
        printf("  Backup Service Area...\n");
        backup_path_ = SABackup::backup_sa(
            handler.get(),
            dev.serial_str().c_str(),
            dev.model_str().c_str()
        );
        if (backup_path_.empty()) {
            printf("  AVERTISSEMENT : backup SA echoue.\n");
            if (!Config::get().dry_run) {
                printf("  Utilisez --force pour continuer sans backup.\n");
                return UnlockResult::BACKUP_FAIL;
            }
        } else {
            printf("  Backup cree : %s\n", backup_path_.c_str());
        }
    }

    /* Mode dry-run : on s'arrete ici */
    if (Config::get().dry_run) {
        printf("  [DRY-RUN] Simulation terminee — aucune ecriture effectuee.\n");
        return UnlockResult::DRY_RUN;
    }

    /* 3. Entrer en mode vendor + RAII guard */
    if (!handler->enter_vendor_mode())
        return UnlockResult::VENDOR_MODE_FAIL;

    VendorModeGuard guard(handler.get());

    /* 4. Patch RAM */
    printf("  Application du patch RAM...\n");
    if (!handler->patch_security_ram()) {
        /* Fallback : tenter la lecture de password SA */
        printf("  Patch RAM echoue. Tentative de lecture des passwords...\n");
        goto fallback_password;
    }

    /* 5. SECURITY DISABLE PASSWORD avec buffer zero */
    printf("  Envoi SECURITY DISABLE PASSWORD (0xF6)...\n");
    if (!send_security_disable()) {
        printf("  SECURITY DISABLE echoue. Tentative via password...\n");
        goto fallback_password;
    }

    /* 6. Verification finale */
    dev.refresh_identify();
    if (!dev.is_locked()) {
        printf("  Disque deverrouille avec succes !\n");
        return UnlockResult::SUCCESS;
    }

    /* Si toujours lock, tenter le fallback password */

fallback_password:
    /* 7. Lire les passwords SA */
    {
        uint8_t sa_buf[512]{};
        if (handler->read_sa_module(WD_SA_MODULE_PASSWORD, sa_buf)) {
            pwd_info_ = SAParser::parse_wd_password_module(sa_buf, sizeof(sa_buf));

            if (pwd_info_.found && !pwd_info_.user_password.empty()) {
                printf("  Password trouve dans la SA. Tentative d'unlock...\n");

                /* 8. SECURITY UNLOCK + SECURITY DISABLE */
                uint8_t pwd_bytes[32]{};
                size_t copy_len = std::min(pwd_info_.user_password.size(), size_t(32));
                memcpy(pwd_bytes, pwd_info_.user_password.data(), copy_len);

                if (send_security_unlock(pwd_bytes)) {
                    send_security_disable(pwd_bytes);

                    dev.refresh_identify();
                    if (!dev.is_locked())
                        return UnlockResult::PASSWORD_UNLOCK;
                }
            }
        }
    }

    /* Dernier recours : envoyer SECURITY DISABLE avec password zero */
    send_security_disable();
    dev.refresh_identify();

    return dev.is_locked() ? UnlockResult::STILL_LOCKED : UnlockResult::SUCCESS;
}
