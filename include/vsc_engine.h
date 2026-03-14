#pragma once
/*
 * vsc_engine.h — Moteur de detection constructeur + factory (F05)
 */

#include "vendor_handler.h"
#include "ata_defs.h"

#include <memory>
#include <string>

/* Convertit un champ ATA word-swapped en std::string propre (trim + swap) */
std::string ata_str_to_utf8(const char* raw, size_t len);

/* ==========================================================================
 *  VSCEngine — Detection constructeur + creation du handler
 * ========================================================================== */

class VSCEngine {
public:
    explicit VSCEngine(ATAInterface* ata) : ata_(ata) {}

    /*
     * detect_vendor — Analyse model_number de IdentifyData.
     * Retourne le handler correspondant ou nullptr si inconnu.
     *
     * Priorite : WD > Seagate > Toshiba > HGST
     */
    std::unique_ptr<VendorHandler> detect_vendor(const IdentifyData& id);

private:
    ATAInterface* ata_;
};
