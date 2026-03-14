/*
 * wd_handler.cpp — Western Digital VSC (0xE0/0xE1/0x45/0x46) (F06)
 */

#include "vendor_handler.h"
#include "config.h"
#include "ata_defs.h"

#include <cstring>
#include <cstdio>
#include <memory>

/* ==========================================================================
 *  Derivation de la cle WD a partir du serial
 * ========================================================================== */

static void compute_wd_key(const char* serial, size_t serial_len, uint8_t key[3]) {
    /*
     * La cle WD est derivee du serial number par XOR.
     * Chaque byte de la cle est le XOR de portions du serial.
     * C'est une protection faible par obscurite (security through obscurity).
     */
    key[0] = key[1] = key[2] = 0;
    for (size_t i = 0; i < serial_len && serial[i] != '\0'; ++i) {
        key[i % 3] ^= static_cast<uint8_t>(serial[i]);
    }
}

/* ==========================================================================
 *  WDHandler
 * ========================================================================== */

class WDHandler final : public VendorHandler {
public:
    explicit WDHandler(ATAInterface* ata) : VendorHandler(ata) {}

    const char* vendor_name() const override { return "Western Digital"; }

    bool enter_vendor_mode() override {
        /* Recuperer le serial pour deriver la cle */
        IdentifyData id{};
        ATAError err = ata_->identify_device(id);
        if (err != ATAError::OK) return false;

        char serial[22]{};
        ata_string_fixup(id.serial_number, sizeof(id.serial_number), serial);

        uint8_t key[3];
        compute_wd_key(serial, strlen(serial), key);

        ATACommand cmd{};
        cmd.command  = WD_CMD_VENDOR_ENTER;   /* 0xE0 */
        cmd.features = 0x44;
        cmd.lba_low  = key[0];
        cmd.lba_mid  = key[1];
        cmd.lba_high = key[2];

        if (Config::get().dry_run && Config::is_write_command(cmd.command)) {
            printf("  [DRY-RUN] WD Enter Vendor Mode (0xE0) - simule\n");
            return true;
        }

        return ata_->send_command(cmd) == ATAError::OK;
    }

    bool exit_vendor_mode() override {
        ATACommand cmd{};
        cmd.command = WD_CMD_VENDOR_EXIT;   /* 0xE1 */

        if (Config::get().dry_run && Config::is_write_command(cmd.command)) {
            printf("  [DRY-RUN] WD Exit Vendor Mode (0xE1) - simule\n");
            return true;
        }

        return ata_->send_command(cmd) == ATAError::OK;
    }

    bool read_sa_module(uint8_t module_id, std::span<uint8_t> buf) override {
        if (buf.size() < 512) return false;

        ATACommand cmd{};
        cmd.command      = WD_CMD_READ_SA_MODULE;   /* 0x45 */
        cmd.features     = module_id;
        cmd.sector_count = static_cast<uint8_t>(buf.size() / 512);
        cmd.buffer       = buf.data();
        cmd.data_size    = buf.size();
        cmd.write        = 0;   /* lecture */

        return ata_->send_command(cmd) == ATAError::OK;
    }

    bool patch_security_ram() override {
        ATACommand cmd{};
        cmd.command      = WD_CMD_WRITE_RAM;   /* 0x46 */
        cmd.features     = 0x00;
        cmd.lba_low      = 0x80;    /* adresse RAM flag security */
        cmd.sector_count = 0x00;    /* valeur : UNLOCKED         */

        if (Config::get().dry_run) {
            printf("  [DRY-RUN] WD Write RAM (0x46) addr=0x80 val=0x00 - simule\n");
            return true;
        }

        return ata_->send_command(cmd) == ATAError::OK;
    }
};

/* Factory */
std::unique_ptr<VendorHandler> make_wd_handler(ATAInterface* ata) {
    return std::make_unique<WDHandler>(ata);
}
