#pragma once

/*
 * ata_interface.h
 * ──────────────────────────────────────────────────────────────────────────
 * Interface abstraite C++ pour l'envoi de commandes ATA bas niveau.
 *
 * Principe :
 *   - Cette classe ne contient AUCUN code OS-spécifique.
 *   - Deux implémentations concrètes :
 *       WinATAInterface  → os/win_ata.cpp   (DeviceIoControl / IOCTL)
 *       LinuxATAInterface→ os/linux_ata.cpp (ioctl SG_IO)
 *   - La factory statique ATAInterface::create() retourne l'implémentation
 *     correcte selon l'OS détecté à la compilation (#ifdef).
 *
 * Usage :
 *   auto ata = ATAInterface::create();
 *   if (!ata->open("\\\\.\\PhysicalDrive0")) { ... }
 *   IdentifyData id{};
 *   ata->identify_device(id);
 *   ata->close();
 * ──────────────────────────────────────────────────────────────────────────
 */

#include "ata_defs.h"

#include <memory>
#include <string>
#include <string_view>
#include <cstdint>

/* ══════════════════════════════════════════════════════════════════════════
 *  CODES D'ERREUR
 * ══════════════════════════════════════════════════════════════════════════ */

enum class ATAError : int {
    OK              =  0,
    NOT_OPEN        = -1,  /* handle non ouvert                             */
    PERMISSION      = -2,  /* droits insuffisants (non admin/root)          */
    DEVICE_NOT_FOUND= -3,  /* chemin introuvable                            */
    IO_ERROR        = -4,  /* erreur I/O lors de l'envoi de la commande     */
    TIMEOUT         = -5,  /* timeout disque                                */
    ABORTED         = -6,  /* commande rejetée par le disque (ABRT bit)     */
    BUFFER_TOO_SMALL= -7,  /* buffer fourni insuffisant                     */
    UNSUPPORTED     = -8,  /* commande non supportée par cet OS/driver      */
};

const char* ata_error_str(ATAError e);

/* ══════════════════════════════════════════════════════════════════════════
 *  CLASSE ABSTRAITE ATAInterface
 * ══════════════════════════════════════════════════════════════════════════ */

class ATAInterface {
public:
    virtual ~ATAInterface() = default;

    /* Non copyable — une interface représente un handle unique */
    ATAInterface(const ATAInterface&)            = delete;
    ATAInterface& operator=(const ATAInterface&) = delete;

    /* ── Gestion du handle ── */

    /**
     * open - Ouvre un accès exclusif au disque physique.
     *
     * @param device_path  Chemin vers le disque :
     *                     Windows : "\\\\.\\PhysicalDrive0"
     *                     Linux   : "/dev/sda" ou "/dev/sg0"
     * @return ATAError::OK si succès, code d'erreur sinon.
     *
     * ⚠ Requiert des droits élevés (Administrateur / root).
     */
    virtual ATAError open(std::string_view device_path) = 0;

    /**
     * close - Ferme le handle proprement. Idempotent (appelable plusieurs fois).
     */
    virtual void close() noexcept = 0;

    /**
     * is_open - Retourne true si le handle est ouvert et valide.
     */
    virtual bool is_open() const noexcept = 0;

    /**
     * device_path - Retourne le chemin du disque ouvert (vide si fermé).
     */
    virtual std::string device_path() const noexcept = 0;

    /* ── Envoi de commandes ── */

    /**
     * send_command - Envoie une commande ATA brute au disque.
     *
     * @param cmd  Structure ATACommand remplie par l'appelant.
     *             Si cmd.data_size > 0, cmd.buffer doit pointer vers un
     *             buffer valide de taille >= cmd.data_size.
     *
     * @return ATAError::OK si la commande a été acceptée par le disque,
     *         code d'erreur sinon.
     *
     * ⚠ Cette fonction ne vérifie PAS le contenu des données retournées.
     *   La validation sémantique est à la charge de l'appelant.
     */
    virtual ATAError send_command(ATACommand& cmd) = 0;

    /* ── Commandes de haut niveau (construites sur send_command) ── */

    /**
     * identify_device - Envoie ATA IDENTIFY DEVICE (0xEC) et remplit out.
     *
     * @param out  Structure IdentifyData à remplir (512 bytes).
     * @return ATAError::OK si succès.
     */
    virtual ATAError identify_device(IdentifyData& out);

    /**
     * last_error - Retourne le dernier code d'erreur OS (GetLastError / errno).
     * Utile pour les messages de debug.
     */
    virtual uint32_t last_os_error() const noexcept = 0;

    /* ── Factory ── */

    /**
     * create - Crée l'implémentation correcte selon l'OS.
     *
     *   Sous Windows : retourne un WinATAInterface
     *   Sous Linux   : retourne un LinuxATAInterface
     *
     * @return unique_ptr vers l'instance créée.
     */
    static std::unique_ptr<ATAInterface> create();

protected:
    ATAInterface() = default;
};

/* ══════════════════════════════════════════════════════════════════════════
 *  RAII WRAPPER — fermeture automatique du handle
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * ATAHandle - Wrapper RAII autour d'ATAInterface.
 * Garantit que close() est appelé même en cas d'exception.
 *
 * Usage :
 *   ATAHandle h(ATAInterface::create());
 *   if (h.open("\\\\.\\PhysicalDrive0") != ATAError::OK) { ... }
 *   // h.close() appelé automatiquement au destructeur
 */
class ATAHandle {
public:
    explicit ATAHandle(std::unique_ptr<ATAInterface> ata)
        : ata_(std::move(ata)) {}

    ~ATAHandle() {
        if (ata_ && ata_->is_open())
            ata_->close();
    }

    ATAHandle(ATAHandle&&)            = default;
    ATAHandle& operator=(ATAHandle&&) = default;
    ATAHandle(const ATAHandle&)       = delete;
    ATAHandle& operator=(const ATAHandle&) = delete;

    ATAInterface* get()       noexcept { return ata_.get(); }
    ATAInterface* operator->() noexcept { return ata_.get(); }

    ATAError open(std::string_view path) { return ata_->open(path); }

private:
    std::unique_ptr<ATAInterface> ata_;
};