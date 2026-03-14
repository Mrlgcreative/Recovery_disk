#pragma once
/*
 * device.h — Classe Device : disque physique avec cache IDENTIFY (F03, F15)
 */

#include "ata_interface.h"
#include "ata_defs.h"

#include <string>
#include <cstring>

class Device {
public:
    explicit Device(ATAInterface* ata) : ata_(ata) {}

    /* Rafraichit les donnees IDENTIFY DEVICE depuis le disque */
    bool refresh_identify() {
        ATAError err = ata_->identify_device(id_);
        if (err != ATAError::OK) return false;
        id_valid_ = true;
        return true;
    }

    /* Accesseurs IdentifyData */
    const IdentifyData& identify() const { return id_; }
    bool has_identify() const { return id_valid_; }

    /* Charger des donnees IDENTIFY externes (ex: fallback IOCTL) */
    void set_identify(const IdentifyData& id) { id_ = id; id_valid_ = true; }

    /* Security status helpers */
    uint16_t security_status() const { return id_.security_status; }
    bool is_locked()    const { return (id_.security_status & SEC_FLAG_LOCKED)  != 0; }
    bool is_frozen()    const { return (id_.security_status & SEC_FLAG_FROZEN)  != 0; }
    bool is_enabled()   const { return (id_.security_status & SEC_FLAG_ENABLED) != 0; }
    bool is_supported() const { return (id_.security_status & SEC_FLAG_SUPPORTED) != 0; }
    bool is_expired()   const { return (id_.security_status & SEC_FLAG_COUNT_EXPIRED) != 0; }

    /* Serial propre (word-swapped + trim) */
    std::string serial_str() const {
        char buf[22]{};
        ata_string_fixup(id_.serial_number, sizeof(id_.serial_number), buf);
        return buf;
    }

    /* Model propre */
    std::string model_str() const {
        char buf[42]{};
        ata_string_fixup(id_.model_number, sizeof(id_.model_number), buf);
        return buf;
    }

    ATAInterface* ata() { return ata_; }

    /* ==== F15 : Gestion FROZEN ==== */

    /*
     * attempt_unfreeze — Tente de degeler le disque.
     * Linux  : suspend-to-RAM via /sys/power/state
     * Windows: affiche un message au technicien
     * Retourne true si le disque n'est plus FROZEN apres la tentative.
     */
    bool attempt_unfreeze();

private:
    ATAInterface* ata_;
    IdentifyData  id_{};
    bool          id_valid_ = false;
};
