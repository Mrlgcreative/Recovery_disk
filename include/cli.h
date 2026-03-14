#pragma once

/*
 * cli.h
 * --------------------------------------------------------------------------
 * Interface en ligne de commande pour le HDD Password Recovery Tool.
 *
 * Responsabilites :
 *   - Enumerer les disques physiques detectes
 *   - Afficher les informations IDENTIFY DEVICE
 *   - Diagnostiquer le statut de securite ATA
 *   - Detecter le constructeur (VSC engine)
 *   - Deverrouillage via RAM patch / password SA
 *   - Backup SA, extraction passwords, rapport
 *
 * Toutes les sorties sont en ASCII pur pour compatibilite console Windows.
 * --------------------------------------------------------------------------
 */

#include "ata_interface.h"
#include "ata_defs.h"

#include <string>
#include <vector>

/* ==========================================================================
 *  DeviceScanner - Enumeration des disques physiques
 * ========================================================================== */

class DeviceScanner {
public:
    static std::vector<std::string> scan();
    static bool probe_device(const std::string& path, DeviceInfo& info);

private:
    static bool probe_via_os_ioctl(const std::string& path, DeviceInfo& info);
};

/* ==========================================================================
 *  CLI - Interface utilisateur interactive
 * ========================================================================== */

class CLI {
public:
    int run(int argc, char* argv[]);

private:
    /* Affichage */
    static void print_banner();
    static void print_help();
    static void print_device_table(const std::vector<DeviceInfo>& devices);
    static void print_device_detail(const DeviceInfo& info);
    static void print_security_analysis(const DeviceInfo& info);

    /* Actions */
    void cmd_scan();
    void cmd_select(int index);
    void cmd_info();
    void cmd_security();
    void cmd_identify_raw();
    void cmd_detect();         /* F05 : detection constructeur   */
    void cmd_unlock();         /* F10 : deverrouillage           */
    void cmd_backup();         /* F13 : backup SA                */
    void cmd_passwords();      /* F14 : extraction passwords SA  */

    /* Etat */
    std::vector<DeviceInfo> devices_;
    int selected_ = -1;

    /* Helpers */
    static std::string security_verdict(uint16_t status);
};
