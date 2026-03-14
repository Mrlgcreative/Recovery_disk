#pragma once
/*
 * vendor_handler.h — Interface abstraite pour les handlers constructeur (F05)
 *
 * Chaque constructeur (WD, Seagate, Toshiba, HGST) implemente cette interface
 * pour acceder aux commandes VSC (Vendor Specific Commands) propriétaires.
 */

#include "ata_interface.h"
#include "ata_defs.h"

#include <cstdint>
#include <span>
#include <string>

/* ==========================================================================
 *  VendorHandler — Classe abstraite
 * ========================================================================== */

class VendorHandler {
public:
    explicit VendorHandler(ATAInterface* ata) : ata_(ata) {}
    virtual ~VendorHandler() = default;

    VendorHandler(const VendorHandler&) = delete;
    VendorHandler& operator=(const VendorHandler&) = delete;

    /* Nom du constructeur ("Western Digital", "Seagate", ...) */
    virtual const char* vendor_name() const = 0;

    /* Entrer en mode vendor (VSC) — obligatoire avant toute operation SA */
    virtual bool enter_vendor_mode() = 0;

    /* Sortir du mode vendor — TOUJOURS appeler (RAII VendorModeGuard) */
    virtual bool exit_vendor_mode() = 0;

    /* Lire un module Service Area dans buf */
    virtual bool read_sa_module(uint8_t module_id, std::span<uint8_t> buf) = 0;

    /* Patcher le flag securite en RAM pour decoller le lock */
    virtual bool patch_security_ram() = 0;

protected:
    ATAInterface* ata_;
};

/* ==========================================================================
 *  VendorModeGuard — RAII : exit_vendor_mode() au destructeur
 * ========================================================================== */

class VendorModeGuard {
public:
    explicit VendorModeGuard(VendorHandler* handler)
        : handler_(handler) {}

    ~VendorModeGuard() {
        if (handler_)
            handler_->exit_vendor_mode();
    }

    VendorModeGuard(VendorModeGuard&& o) noexcept
        : handler_(o.handler_) { o.handler_ = nullptr; }
    VendorModeGuard& operator=(VendorModeGuard&&) = delete;
    VendorModeGuard(const VendorModeGuard&) = delete;
    VendorModeGuard& operator=(const VendorModeGuard&) = delete;

    void release() { handler_ = nullptr; }

private:
    VendorHandler* handler_;
};
