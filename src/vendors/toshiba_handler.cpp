/*
 * toshiba_handler.cpp — Toshiba VSC 0xC0/0xC1 (F08)
 */

#include "vendor_handler.h"
#include "config.h"
#include "ata_defs.h"

#include <cstdio>
#include <memory>

/* ==========================================================================
 *  ToshibaHandler
 * ========================================================================== */

class ToshibaHandler final : public VendorHandler {
public:
    explicit ToshibaHandler(ATAInterface* ata) : VendorHandler(ata) {}

    const char* vendor_name() const override { return "Toshiba"; }

    bool enter_vendor_mode() override {
        ATACommand cmd{};
        cmd.command = TOSH_CMD_VENDOR_ENTER;   /* 0xC0 */

        if (Config::get().dry_run && Config::is_write_command(cmd.command)) {
            printf("  [DRY-RUN] Toshiba Enter Vendor Diag Mode (0xC0) - simule\n");
            return true;
        }

        return ata_->send_command(cmd) == ATAError::OK;
    }

    bool exit_vendor_mode() override {
        ATACommand cmd{};
        cmd.command = TOSH_CMD_VENDOR_EXIT;   /* 0xC1 */

        if (Config::get().dry_run && Config::is_write_command(cmd.command)) {
            printf("  [DRY-RUN] Toshiba Exit Vendor Diag Mode (0xC1) - simule\n");
            return true;
        }

        return ata_->send_command(cmd) == ATAError::OK;
    }

    bool read_sa_module(uint8_t module_id, std::span<uint8_t> buf) override {
        if (buf.size() < 512) return false;

        ATACommand cmd{};
        cmd.command      = TOSH_CMD_READ_SA;   /* 0xC4 */
        cmd.features     = module_id;
        cmd.sector_count = static_cast<uint8_t>(buf.size() / 512);
        cmd.buffer       = buf.data();
        cmd.data_size    = buf.size();
        cmd.write        = 0;

        return ata_->send_command(cmd) == ATAError::OK;
    }

    bool patch_security_ram() override {
        /*
         * Toshiba : ecriture RAM via commande VSC proprietaire.
         * L'adresse et la valeur sont specifiques a la generation firmware.
         * On utilise une approche generique avec 0xC4 en ecriture.
         */
        uint8_t zero_buf[512]{};

        ATACommand cmd{};
        cmd.command      = 0xC4;
        cmd.features     = 0x00;   /* sous-commande patch security */
        cmd.sector_count = 1;
        cmd.buffer       = zero_buf;
        cmd.data_size    = 512;
        cmd.write        = 1;

        if (Config::get().dry_run) {
            printf("  [DRY-RUN] Toshiba RAM patch security - simule\n");
            return true;
        }

        return ata_->send_command(cmd) == ATAError::OK;
    }
};

/* Factory */
std::unique_ptr<VendorHandler> make_toshiba_handler(ATAInterface* ata) {
    return std::make_unique<ToshibaHandler>(ata);
}
