/*
 * win_ata.cpp
 * ──────────────────────────────────────────────────────────────────────────
 * Implémentation Windows de ATAInterface.
 * Utilise DeviceIoControl() avec IOCTL_ATA_PASS_THROUGH_EX pour envoyer
 * des commandes ATA 28 bits directement au contrôleur SATA/IDE.
 *
 * Prérequis :
 *   - Windows 10 ou supérieur (64-bit)
 *   - Exécution en tant qu'Administrateur
 *   - Lien avec : ntdll.lib, setupapi.lib, cfgmgr32.lib
 *
 * Headers requis : <windows.h>, <ntddscsi.h>, <devioctl.h>
 * ──────────────────────────────────────────────────────────────────────────
 */

#ifdef _WIN32

#include "ata_interface.h"

/* Windows headers — ordre important */
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winioctl.h>   /* IOCTL_* definitions           */
#include <ntddscsi.h>   /* ATA_PASS_THROUGH_EX, ATA_FLAGS_* */

#include <cstring>
#include <string>
#include <cassert>

/* ══════════════════════════════════════════════════════════════════════════
 *  CONSTANTES WINDOWS ATA
 * ══════════════════════════════════════════════════════════════════════════ */

/* Taille du buffer interne : header + données (512 bytes max par commande) */
static constexpr size_t WIN_ATA_BUFFER_SIZE = sizeof(ATA_PASS_THROUGH_EX) + 512;

/* Task File Register indices dans CurrentTaskFile[] */
static constexpr int TFR_FEATURES     = 0;
static constexpr int TFR_SECTOR_COUNT = 1;
static constexpr int TFR_LBA_LOW      = 2;
static constexpr int TFR_LBA_MID      = 3;
static constexpr int TFR_LBA_HIGH     = 4;
static constexpr int TFR_DEVICE       = 5;
static constexpr int TFR_COMMAND      = 6;

/* ══════════════════════════════════════════════════════════════════════════
 *  CLASSE WinATAInterface
 * ══════════════════════════════════════════════════════════════════════════ */

class WinATAInterface final : public ATAInterface {
public:
    WinATAInterface() : handle_(INVALID_HANDLE_VALUE), last_os_err_(0) {}

    ~WinATAInterface() override {
        close();
    }

    /* ── open ─────────────────────────────────────────────────────────── */

    ATAError open(std::string_view device_path) override {
        if (handle_ != INVALID_HANDLE_VALUE)
            close();

        device_path_ = std::string(device_path);

        /*
         * Ouverture en accès exclusif avec GENERIC_READ | GENERIC_WRITE.
         * FILE_SHARE_READ | FILE_SHARE_WRITE permet aux autres apps de lire
         * pendant qu'on a le handle (nécessaire pour éviter un deadlock OS).
         */
        handle_ = CreateFileA(
            device_path_.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        if (handle_ == INVALID_HANDLE_VALUE) {
            last_os_err_ = GetLastError();
            switch (last_os_err_) {
                case ERROR_ACCESS_DENIED:  return ATAError::PERMISSION;
                case ERROR_FILE_NOT_FOUND:
                case ERROR_PATH_NOT_FOUND: return ATAError::DEVICE_NOT_FOUND;
                default:                   return ATAError::IO_ERROR;
            }
        }

        return ATAError::OK;
    }

    /* ── close ────────────────────────────────────────────────────────── */

    void close() noexcept override {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
        device_path_.clear();
    }

    /* ── is_open ──────────────────────────────────────────────────────── */

    bool is_open() const noexcept override {
        return handle_ != INVALID_HANDLE_VALUE;
    }

    /* ── device_path ──────────────────────────────────────────────────── */

    std::string device_path() const noexcept override {
        return device_path_;
    }

    /* ── last_os_error ────────────────────────────────────────────────── */

    uint32_t last_os_error() const noexcept override {
        return last_os_err_;
    }

    /* ── send_command ─────────────────────────────────────────────────── */

    /**
     * Construit une structure ATA_PASS_THROUGH_EX et l'envoie via
     * DeviceIoControl(IOCTL_ATA_PASS_THROUGH).
     *
     * La structure ATA_PASS_THROUGH_EX contient :
     *   [0..sizeof(APT_EX)-1] : header de contrôle
     *   [sizeof(APT_EX)..]    : données I/O (si data_size > 0)
     *
     * Le buffer est alloué sur la pile pour éviter toute allocation heap
     * dans le chemin critique.
     */
    ATAError send_command(ATACommand& cmd) override {
        if (!is_open()) return ATAError::NOT_OPEN;

        /* Vérification cohérence buffer */
        if (cmd.data_size > 0 && cmd.buffer == nullptr)
            return ATAError::BUFFER_TOO_SMALL;

        /* ── Construction ATA_PASS_THROUGH_EX ── */

        /*
         * On alloue un buffer local contigu : header + data.
         * DeviceIoControl attend que les données soient juste après le header
         * (DataBufferOffset pointe vers cet espace).
         */
        alignas(ATA_PASS_THROUGH_EX)
        uint8_t local_buf[sizeof(ATA_PASS_THROUGH_EX) + 512] = {};

        auto* apt = reinterpret_cast<ATA_PASS_THROUGH_EX*>(local_buf);

        apt->Length             = sizeof(ATA_PASS_THROUGH_EX);
        apt->TimeOutValue       = 30;  /* 30 secondes — suffisant pour SA reads */
        apt->DataTransferLength = static_cast<ULONG>(cmd.data_size);
        apt->DataBufferOffset   = sizeof(ATA_PASS_THROUGH_EX);

        /* Direction du transfert */
        if (cmd.data_size == 0) {
            apt->AtaFlags = ATA_FLAGS_DRDY_REQUIRED;
        } else if (cmd.write) {
            apt->AtaFlags = ATA_FLAGS_DRDY_REQUIRED | ATA_FLAGS_DATA_OUT;
        } else {
            apt->AtaFlags = ATA_FLAGS_DRDY_REQUIRED | ATA_FLAGS_DATA_IN;
        }

        /* Task File Registers */
        apt->CurrentTaskFile[TFR_FEATURES]     = cmd.features;
        apt->CurrentTaskFile[TFR_SECTOR_COUNT] = cmd.sector_count;
        apt->CurrentTaskFile[TFR_LBA_LOW]      = cmd.lba_low;
        apt->CurrentTaskFile[TFR_LBA_MID]      = cmd.lba_mid;
        apt->CurrentTaskFile[TFR_LBA_HIGH]     = cmd.lba_high;
        apt->CurrentTaskFile[TFR_DEVICE]       = cmd.device ? cmd.device : 0xA0;
        apt->CurrentTaskFile[TFR_COMMAND]      = cmd.command;

        /* Copier les données d'entrée si écriture */
        if (cmd.write && cmd.data_size > 0 && cmd.buffer != nullptr) {
            std::memcpy(local_buf + sizeof(ATA_PASS_THROUGH_EX),
                        cmd.buffer, cmd.data_size);
        }

        /* ── Envoi via DeviceIoControl ── */

        DWORD bytes_returned = 0;
        const DWORD buf_size = sizeof(ATA_PASS_THROUGH_EX) +
                               static_cast<DWORD>(cmd.data_size);

        BOOL ok = DeviceIoControl(
            handle_,
            IOCTL_ATA_PASS_THROUGH,
            local_buf, buf_size,
            local_buf, buf_size,
            &bytes_returned,
            nullptr          /* synchrone — pas d'OVERLAPPED */
        );

        if (!ok) {
            last_os_err_ = GetLastError();
            return ATAError::IO_ERROR;
        }

        /*
         * Vérification du status ATA retourné.
         * Si le bit ERR (bit 0) du Status Register est levé,
         * on vérifie le bit ABRT dans Error Register.
         */
        const uint8_t status_reg = apt->CurrentTaskFile[6]; /* Status */
        const uint8_t error_reg  = apt->CurrentTaskFile[0]; /* Error  */

        if (status_reg & 0x01) {          /* ERR bit */
            if (error_reg & 0x04)         /* ABRT bit */
                return ATAError::ABORTED;
            return ATAError::IO_ERROR;
        }

        /* Copier les données retournées si lecture */
        if (!cmd.write && cmd.data_size > 0 && cmd.buffer != nullptr) {
            std::memcpy(cmd.buffer,
                        local_buf + sizeof(ATA_PASS_THROUGH_EX),
                        cmd.data_size);
        }

        last_os_err_ = 0;
        return ATAError::OK;
    }

private:
    HANDLE      handle_;
    std::string device_path_;
    DWORD       last_os_err_;
};

/* ══════════════════════════════════════════════════════════════════════════
 *  FACTORY — appelée par ATAInterface::create() via ata_interface.cpp
 * ══════════════════════════════════════════════════════════════════════════ */

std::unique_ptr<ATAInterface> make_win_ata_interface() {
    return std::make_unique<WinATAInterface>();
}

#endif /* _WIN32 */