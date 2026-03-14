/*
 * vsc_engine.cpp — Detection constructeur + helpers string ATA (F05)
 */

#include "vsc_engine.h"
#include "config.h"

#include <cstring>
#include <algorithm>

/* Forward declarations des handlers */
class WDHandler;
class SeagateHandler;
class ToshibaHandler;
class HGSTHandler;

/* Factories declarees dans chaque fichier vendor */
std::unique_ptr<VendorHandler> make_wd_handler(ATAInterface* ata);
std::unique_ptr<VendorHandler> make_seagate_handler(ATAInterface* ata);
std::unique_ptr<VendorHandler> make_toshiba_handler(ATAInterface* ata);
std::unique_ptr<VendorHandler> make_hgst_handler(ATAInterface* ata);

/* ==========================================================================
 *  ata_str_to_utf8 — Convertit un champ ATA word-swapped en std::string
 * ========================================================================== */

std::string ata_str_to_utf8(const char* raw, size_t len) {
    std::string result(len, '\0');

    /* Swap chaque paire de bytes */
    for (size_t i = 0; i + 1 < len; i += 2) {
        result[i]     = raw[i + 1];
        result[i + 1] = raw[i];
    }
    if (len % 2 != 0)
        result[len - 1] = raw[len - 1];

    /* Trim trailing spaces et nuls */
    auto end = result.find_last_not_of(" \0", std::string::npos);
    if (end != std::string::npos)
        result.resize(end + 1);
    else
        result.clear();

    return result;
}

/* ==========================================================================
 *  VSCEngine::detect_vendor
 * ========================================================================== */

std::unique_ptr<VendorHandler> VSCEngine::detect_vendor(const IdentifyData& id) {
    std::string model = ata_str_to_utf8(id.model_number, sizeof(id.model_number));

    /* Western Digital : prefixes WDC ou WD */
    if (model.starts_with("WDC ") || model.starts_with("WD "))
        return make_wd_handler(ata_);

    /* Seagate : prefixes ST ou Seagate */
    if (model.starts_with("ST") || model.starts_with("Seagate"))
        return make_seagate_handler(ata_);

    /* Toshiba : prefixes TOSHIBA ou THNS (SSD Toshiba) */
    if (model.starts_with("TOSHIBA") || model.starts_with("THNS"))
        return make_toshiba_handler(ata_);

    /* HGST (rachete par WD) : HUA, HDS, HTS */
    if (model.starts_with("HUA") || model.starts_with("HDS") || model.starts_with("HTS"))
        return make_hgst_handler(ata_);

    return nullptr;
}
