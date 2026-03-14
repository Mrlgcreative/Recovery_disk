/*
 * ata_interface.cpp
 * ──────────────────────────────────────────────────────────────────────────
 * Implémentation des méthodes partagées de ATAInterface :
 *   - identify_device()  : construit la commande ATA 0xEC et appelle send_command()
 *   - ata_error_str()    : description textuelle des codes d'erreur
 *   - ATAInterface::create() : factory OS-agnostique
 *
 * Les implémentations OS-spécifiques (WinATAInterface, LinuxATAInterface)
 * sont dans os/win_ata.cpp et os/linux_ata.cpp.
 * ──────────────────────────────────────────────────────────────────────────
 */

#include "ata_interface.h"

#include <cstring>
#include <stdexcept>

/* ── Implémentations concrètes — déclarées ici, définies dans os/ ── */
#ifdef _WIN32
    class WinATAInterface;
    std::unique_ptr<ATAInterface> make_win_ata_interface();
#elif defined(__linux__)
    class LinuxATAInterface;
    std::unique_ptr<ATAInterface> make_linux_ata_interface();
#else
    #error "Système d'exploitation non supporté. Compilez sous Windows ou Linux."
#endif

/* ══════════════════════════════════════════════════════════════════════════
 *  FACTORY — sélection de l'implémentation OS
 * ══════════════════════════════════════════════════════════════════════════ */

std::unique_ptr<ATAInterface> ATAInterface::create() {
#ifdef _WIN32
    return make_win_ata_interface();
#elif defined(__linux__)
    return make_linux_ata_interface();
#else
    throw std::runtime_error("ATAInterface::create() : OS non supporté.");
#endif
}

/* ══════════════════════════════════════════════════════════════════════════
 *  identify_device — implémentation générique (utilise send_command)
 *
 *  ATA IDENTIFY DEVICE (0xEC) :
 *    - Pas de LBA, pas de features spéciales
 *    - Transfert HOST←DEV de 512 bytes
 *    - Le résultat est une IdentifyData word-swapped
 * ══════════════════════════════════════════════════════════════════════════ */

ATAError ATAInterface::identify_device(IdentifyData& out) {
    std::memset(&out, 0, sizeof(IdentifyData));

    ATACommand cmd{};
    cmd.command   = ATA_CMD_IDENTIFY_DEVICE;  /* 0xEC                     */
    cmd.device    = 0xA0;                     /* sélection device (master)*/
    cmd.buffer    = reinterpret_cast<uint8_t*>(&out);
    cmd.data_size = sizeof(IdentifyData);     /* 512 bytes                */
    cmd.write     = 0;                        /* lecture HOST←DEV         */

    return send_command(cmd);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  ata_error_str — description textuelle des codes ATAError
 * ══════════════════════════════════════════════════════════════════════════ */

const char* ata_error_str(ATAError e) {
    switch (e) {
        case ATAError::OK:               return "OK";
        case ATAError::NOT_OPEN:         return "Handle non ouvert";
        case ATAError::PERMISSION:       return "Droits insuffisants (lancez en admin/root)";
        case ATAError::DEVICE_NOT_FOUND: return "Peripherique introuvable";
        case ATAError::IO_ERROR:         return "Erreur I/O";
        case ATAError::TIMEOUT:          return "Timeout disque";
        case ATAError::ABORTED:          return "Commande rejetee par le disque (ABRT)";
        case ATAError::BUFFER_TOO_SMALL: return "Buffer trop petit";
        case ATAError::UNSUPPORTED:      return "Commande non supportee sur cet OS";
        default:                         return "Erreur inconnue";
    }
}