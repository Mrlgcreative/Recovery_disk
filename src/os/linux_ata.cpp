/*
 * linux_ata.cpp
 * ──────────────────────────────────────────────────────────────────────────
 * Implémentation Linux de ATAInterface.
 * Utilise ioctl(SG_IO) avec le protocole SAT (SCSI/ATA Translation)
 * pour encapsuler les commandes ATA dans des CDB SCSI de 16 bytes.
 *
 * Prérequis :
 *   - Linux kernel 2.6.30+ (sg driver)
 *   - Droits root ou appartenance au groupe 'disk'
 *   - Headers : <scsi/sg.h>, <sys/ioctl.h>
 *
 * Deux interfaces possibles :
 *   /dev/sdX  → accès via le bloc SCSI générique (le plus courant)
 *   /dev/sgX  → accès direct SCSI Generic (recommandé pour les VSC)
 *
 * ──────────────────────────────────────────────────────────────────────────
 */

#ifdef __linux__

#include "ata_interface.h" 

/* POSIX + Linux headers */
#include <fcntl.h>       /* open(), O_RDWR, O_NONBLOCK */
#include <unistd.h>      /* close()                    */
#include <sys/ioctl.h>   /* ioctl()                    */
#include <scsi/sg.h>     /* sg_io_hdr_t, SG_IO         */
#include <errno.h>

#include <cstring>
#include <string>
#include <vector>
#include <array>

/* ══════════════════════════════════════════════════════════════════════════
 *  CONSTANTES SAT — SCSI/ATA Translation
 *  Référence : SAT-4 (INCITS 491-2018)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Opcode SCSI ATA PASS-THROUGH(16) — CDB de 16 bytes */
static constexpr uint8_t  SCSI_ATA_PASSTHROUGH_16 = 0x85;

/* Champ PROTOCOL dans CDB[1] (bits 4:1) */
static constexpr uint8_t  SAT_PROTO_NON_DATA  = (3 << 1);  /* Pas de transfert  */
static constexpr uint8_t  SAT_PROTO_PIO_IN    = (4 << 1);  /* PIO DEV→HOST      */
static constexpr uint8_t  SAT_PROTO_PIO_OUT   = (5 << 1);  /* PIO HOST→DEV      */
static constexpr uint8_t  SAT_PROTO_DMA       = (6 << 1);  /* DMA               */

/* Champ FLAGS dans CDB[2] */
static constexpr uint8_t  SAT_CK_COND    = (1 << 5); /* Check Condition demandé   */
static constexpr uint8_t  SAT_T_DIR_IN   = (1 << 3); /* Direction : DEV→HOST      */
static constexpr uint8_t  SAT_BYTE_BLOCK = (1 << 2); /* T_LENGTH en secteurs      */
static constexpr uint8_t  SAT_T_LEN_STPSIU = 0x02;   /* T_LENGTH from sector_count*/

/* Timeout SG_IO en millisecondes */
static constexpr unsigned SG_TIMEOUT_MS = 30000;  /* 30 secondes */

/* ══════════════════════════════════════════════════════════════════════════
 *  CLASSE LinuxATAInterface
 * ══════════════════════════════════════════════════════════════════════════ */

class LinuxATAInterface final : public ATAInterface {
public:
    LinuxATAInterface() : fd_(-1), last_errno_(0) {}

    ~LinuxATAInterface() override {
        close();
    }

    /* ── open ─────────────────────────────────────────────────────────── */

    ATAError open(std::string_view device_path) override {
        if (fd_ >= 0) close();

        device_path_ = std::string(device_path);

        /*
         * O_RDWR   : nécessaire pour les commandes d'écriture (VSC)
         * O_NONBLOCK : évite de bloquer sur certains périphériques SCSI
         */
        fd_ = ::open(device_path_.c_str(), O_RDWR | O_NONBLOCK);

        if (fd_ < 0) {
            last_errno_ = errno;
            switch (last_errno_) {
                case EACCES:
                case EPERM:  return ATAError::PERMISSION;
                case ENOENT:
                case ENODEV: return ATAError::DEVICE_NOT_FOUND;
                default:     return ATAError::IO_ERROR;
            }
        }

        return ATAError::OK;
    }

    /* ── close ────────────────────────────────────────────────────────── */

    void close() noexcept override {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        device_path_.clear();
    }

    /* ── is_open ──────────────────────────────────────────────────────── */

    bool is_open() const noexcept override {
        return fd_ >= 0;
    }

    /* ── device_path ──────────────────────────────────────────────────── */

    std::string device_path() const noexcept override {
        return device_path_;
    }

    /* ── last_os_error ────────────────────────────────────────────────── */

    uint32_t last_os_error() const noexcept override {
        return static_cast<uint32_t>(last_errno_);
    }

    /* ── send_command ─────────────────────────────────────────────────── */

    /**
     * Encapsule la commande ATA dans un CDB SAT ATA PASS-THROUGH(16)
     * et l'envoie via ioctl(SG_IO).
     *
     * Structure du CDB[16] SAT :
     *   [0]  : 0x85 (ATA PASS-THROUGH 16)
     *   [1]  : MULTIPLE_COUNT | PROTOCOL
     *   [2]  : OFF_LINE | CK_COND | T_DIR | BYTE_BLOCK | T_LENGTH
     *   [3]  : features (15:8)  — 0 pour 28-bit
     *   [4]  : features (7:0)
     *   [5]  : sector_count (15:8) — 0 pour 28-bit
     *   [6]  : sector_count (7:0)
     *   [7]  : LBA (31:24)    — 0 pour 28-bit
     *   [8]  : LBA (23:16)    = lba_high
     *   [9]  : LBA (15:8)     = lba_mid
     *   [10] : LBA (7:0)      = lba_low
     *   [11] : LBA (39:32)    — 0 pour 28-bit
     *   [12] : LBA (47:40)    — 0 pour 28-bit
     *   [13] : device register
     *   [14] : command register
     *   [15] : control (toujours 0)
     */
    ATAError send_command(ATACommand& cmd) override {
        if (!is_open()) return ATAError::NOT_OPEN;
        if (cmd.data_size > 0 && cmd.buffer == nullptr)
            return ATAError::BUFFER_TOO_SMALL;

        /* ── Construction du CDB SAT ── */

        std::array<uint8_t, 16> cdb{};

        cdb[0] = SCSI_ATA_PASSTHROUGH_16;

        /* Protocole selon direction et présence de données */
        if (cmd.data_size == 0) {
            cdb[1] = SAT_PROTO_NON_DATA;
        } else if (cmd.write) {
            cdb[1] = SAT_PROTO_PIO_OUT;
        } else {
            cdb[1] = SAT_PROTO_PIO_IN;
        }

        /* Flags de transfert */
        cdb[2] = SAT_CK_COND;  /* Toujours demander Check Condition pour avoir le status */
        if (cmd.data_size > 0) {
            cdb[2] |= SAT_BYTE_BLOCK | SAT_T_LEN_STPSIU;
            if (!cmd.write) cdb[2] |= SAT_T_DIR_IN;
        }

        /* Registres ATA → champs CDB */
        cdb[4]  = cmd.features;
        cdb[6]  = cmd.sector_count;
        cdb[8]  = cmd.lba_high;
        cdb[9]  = cmd.lba_mid;
        cdb[10] = cmd.lba_low;
        cdb[13] = cmd.device ? cmd.device : 0xA0;
        cdb[14] = cmd.command;

        /* ── Sense buffer pour récupérer le status ATA retourné ── */

        std::array<uint8_t, 32> sense{};

        /* ── Remplissage sg_io_hdr_t ── */

        sg_io_hdr_t io_hdr{};
        io_hdr.interface_id    = 'S';             /* Toujours 'S' pour SG_IO   */
        io_hdr.cmd_len         = static_cast<unsigned char>(cdb.size());
        io_hdr.cmdp            = cdb.data();
        io_hdr.mx_sb_len       = static_cast<unsigned char>(sense.size());
        io_hdr.sbp             = sense.data();
        io_hdr.timeout         = SG_TIMEOUT_MS;

        if (cmd.data_size > 0) {
            io_hdr.dxfer_len = static_cast<unsigned>(cmd.data_size);
            io_hdr.dxferp    = cmd.buffer;
            io_hdr.dxfer_direction = cmd.write ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
        } else {
            io_hdr.dxfer_direction = SG_DXFER_NONE;
        }

        /* ── Envoi ioctl ── */

        if (::ioctl(fd_, SG_IO, &io_hdr) < 0) {
            last_errno_ = errno;
            return ATAError::IO_ERROR;
        }

        /* ── Vérification des erreurs SG ── */

        if (io_hdr.status && io_hdr.status != 0x02 /* CHECK_CONDITION */) {
            /* Status SCSI inattendu */
            return ATAError::IO_ERROR;
        }

        /*
         * Analyse du Sense Data ATA retourné (Descriptor Format sense).
         * En mode SAT, le disque retourne le status ATA dans le sense data
         * au format descriptor type 0x09 (ATA Return Descriptor).
         *
         * sense[0] = 0x72 (Descriptor sense)
         * sense[8] = 0x09 (ATA Return Descriptor)
         * sense[21] = Status register ATA
         * sense[11] = Error register ATA
         */
        if (io_hdr.sb_len_wr >= 22 && sense[0] == 0x72 && sense[8] == 0x09) {
            const uint8_t ata_status = sense[21];
            const uint8_t ata_error  = sense[11];

            if (ata_status & 0x01) {       /* ERR bit */
                if (ata_error & 0x04)      /* ABRT bit */
                    return ATAError::ABORTED;
                return ATAError::IO_ERROR;
            }
        }

        /* Succès */
        last_errno_ = 0;
        return ATAError::OK;
    }

private:
    int         fd_;
    std::string device_path_;
    int         last_errno_;
};

/* ══════════════════════════════════════════════════════════════════════════
 *  FACTORY
 * ══════════════════════════════════════════════════════════════════════════ */

std::unique_ptr<ATAInterface> make_linux_ata_interface() {
    return std::make_unique<LinuxATAInterface>();
}

#endif /* __linux__ */