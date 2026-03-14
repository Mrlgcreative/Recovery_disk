/*
 * seagate_handler.cpp — Seagate VSC ATA + F3 UART optionnel (F07)
 */

#include "vendor_handler.h"
#include "config.h"
#include "ata_defs.h"

#include <cstdio>
#include <memory>

/* ==========================================================================
 *  SeagateHandler — Mode ATA VSC (series recentes Barracuda/IronWolf)
 * ========================================================================== */

class SeagateHandler final : public VendorHandler {
public:
    explicit SeagateHandler(ATAInterface* ata) : VendorHandler(ata) {}

    const char* vendor_name() const override { return "Seagate"; }

    bool enter_vendor_mode() override {
        /*
         * Sequence Seagate ATA VSC :
         * Step 1 : SMART vendor enable (0xB0, features=0xD6)
         * Step 2 : Vendor unlock (0x00, LBA_mid=0x4F, LBA_high=0xC2)
         */
        ATACommand s1{};
        s1.command  = SEA_CMD_SMART_VENDOR_ENABLE;   /* 0xB0 */
        s1.features = SEA_FEAT_VENDOR_ENABLE;        /* 0xD6 */
        s1.lba_mid  = SEA_LBA_MID_MAGIC;             /* 0x4F */
        s1.lba_high = SEA_LBA_HIGH_MAGIC;            /* 0xC2 */

        ATACommand s2{};
        s2.command  = SEA_CMD_VENDOR_UNLOCK;          /* 0x00 */
        s2.lba_mid  = SEA_LBA_MID_MAGIC;             /* 0x4F */
        s2.lba_high = SEA_LBA_HIGH_MAGIC;            /* 0xC2 */

        if (Config::get().dry_run) {
            printf("  [DRY-RUN] Seagate SMART Vendor Enable (0xB0/0xD6) - simule\n");
            printf("  [DRY-RUN] Seagate Vendor Unlock (0x00) - simule\n");
            return true;
        }

        ATAError e1 = ata_->send_command(s1);
        if (e1 != ATAError::OK) {
            if (!Config::get().uart_port.empty()) {
                printf("  Mode ATA VSC echoue. Tentez le mode F3 UART sur %s.\n",
                       Config::get().uart_port.c_str());
            }
            return false;
        }

        ATAError e2 = ata_->send_command(s2);
        return e2 == ATAError::OK;
    }

    bool exit_vendor_mode() override {
        /*
         * Seagate : pas de commande explicite de sortie.
         * Le mode vendor se desactive au prochain reset ou power cycle.
         * On envoie quand meme un SMART DISABLE (0xB0/0xD9) par securite.
         */
        ATACommand cmd{};
        cmd.command  = 0xB0;
        cmd.features = 0xD9;   /* SMART DISABLE OPERATIONS */
        cmd.lba_mid  = SEA_LBA_MID_MAGIC;
        cmd.lba_high = SEA_LBA_HIGH_MAGIC;

        if (Config::get().dry_run) {
            printf("  [DRY-RUN] Seagate SMART Disable (0xB0/0xD9) - simule\n");
            return true;
        }

        /* Pas critique si ca echoue */
        ata_->send_command(cmd);
        return true;
    }

    bool read_sa_module(uint8_t module_id, std::span<uint8_t> buf) override {
        if (buf.size() < 512) return false;

        /*
         * Seagate : lecture NVRAM via SMART READ LOG (0xB0, features=0xD5)
         * Le module_id est passe dans sector_count / LBA_low.
         */
        ATACommand cmd{};
        cmd.command      = 0xB0;
        cmd.features     = 0xD5;   /* SMART READ LOG */
        cmd.lba_low      = module_id;
        cmd.lba_mid      = SEA_LBA_MID_MAGIC;
        cmd.lba_high     = SEA_LBA_HIGH_MAGIC;
        cmd.sector_count = static_cast<uint8_t>(buf.size() / 512);
        cmd.buffer       = buf.data();
        cmd.data_size    = buf.size();
        cmd.write        = 0;

        return ata_->send_command(cmd) == ATAError::OK;
    }

    bool patch_security_ram() override {
        /*
         * Seagate : le patch RAM passe par l'ecriture d'un secteur NVRAM
         * modifie via SMART WRITE LOG (0xB0, features=0xD6).
         * On ecrit 512 bytes a zero dans le module security.
         */
        uint8_t zero_buf[512]{};

        ATACommand cmd{};
        cmd.command      = 0xB0;
        cmd.features     = 0xD6;   /* SMART WRITE LOG (vendor) */
        cmd.lba_low      = 0x00;   /* Module security */
        cmd.lba_mid      = SEA_LBA_MID_MAGIC;
        cmd.lba_high     = SEA_LBA_HIGH_MAGIC;
        cmd.sector_count = 1;
        cmd.buffer       = zero_buf;
        cmd.data_size    = 512;
        cmd.write        = 1;

        if (Config::get().dry_run) {
            printf("  [DRY-RUN] Seagate SMART Write Log security patch - simule\n");
            return true;
        }

        return ata_->send_command(cmd) == ATAError::OK;
    }
};

/* Factory */
std::unique_ptr<VendorHandler> make_seagate_handler(ATAInterface* ata) {
    return std::make_unique<SeagateHandler>(ata);
}
