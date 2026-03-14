#pragma once

/*
 * ata_defs.h
 * ──────────────────────────────────────────────────────────────────────────
 * Constantes ATA/ATAPI, opcodes et structures binaires communes.
 * Aucune dépendance externe — pur C standard (compilable en C et C++).
 *
 * Référence : ATA/ATAPI Command Set – 3 (ACS-3), INCITS 522-2014
 * ──────────────────────────────────────────────────────────────────────────
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ══════════════════════════════════════════════════════════════════════════
 *  OPCODES ATA STANDARD
 * ══════════════════════════════════════════════════════════════════════════ */

#define ATA_CMD_IDENTIFY_DEVICE         0xEC  /* Retourne 512 bytes IdentifyData     */
#define ATA_CMD_SECURITY_SET_PASSWORD   0xF1  /* Définir USER ou MASTER password     */
#define ATA_CMD_SECURITY_UNLOCK         0xF2  /* Déverrouiller avec le password      */
#define ATA_CMD_SECURITY_ERASE_PREPARE  0xF3  /* Préparer un effacement sécurisé     */
#define ATA_CMD_SECURITY_ERASE_UNIT     0xF4  /* Effacement complet (DESTRUCTEUR)    */
#define ATA_CMD_SECURITY_FREEZE_LOCK    0xF5  /* Geler l'état de sécurité            */
#define ATA_CMD_SECURITY_DISABLE        0xF6  /* Désactiver le password (unlock)     */
#define ATA_CMD_SMART                   0xB0  /* SMART — sous-commande dans features */

/* ══════════════════════════════════════════════════════════════════════════
 *  OPCODES VSC — WESTERN DIGITAL
 * ══════════════════════════════════════════════════════════════════════════ */

#define WD_CMD_VENDOR_ENTER             0xE0  /* Entrer en mode constructeur WD      */
#define WD_CMD_VENDOR_EXIT              0xE1  /* Sortir du mode constructeur WD      */
#define WD_CMD_READ_SA_MODULE           0x45  /* Lire un module Service Area          */
#define WD_CMD_WRITE_RAM                0x46  /* Écrire dans la RAM du contrôleur    */

#define WD_SA_MODULE_PASSWORD           0x20  /* Module 32 : stocke les passwords    */
#define WD_SA_MODULE_FLAGS              0x2A  /* Module 42 : flags de sécurité       */

/* ══════════════════════════════════════════════════════════════════════════
 *  OPCODES VSC — SEAGATE
 * ══════════════════════════════════════════════════════════════════════════ */

#define SEA_CMD_SMART_VENDOR_ENABLE     0xB0  /* SMART opcode — features = 0xD6      */
#define SEA_FEAT_VENDOR_ENABLE          0xD6  /* Sub-commande activation mode vendor */
#define SEA_CMD_VENDOR_UNLOCK           0x00  /* Unlock vendor mode                  */
#define SEA_LBA_MID_MAGIC               0x4F  /* Valeur LBA_mid requise              */
#define SEA_LBA_HIGH_MAGIC              0xC2  /* Valeur LBA_high requise             */

/* ══════════════════════════════════════════════════════════════════════════
 *  OPCODES VSC — TOSHIBA / HGST
 * ══════════════════════════════════════════════════════════════════════════ */

#define TOSH_CMD_VENDOR_ENTER           0xC0  /* Entrer mode diagnostic Toshiba      */
#define TOSH_CMD_VENDOR_EXIT            0xC1  /* Sortir mode diagnostic Toshiba      */
#define TOSH_CMD_READ_SA                0xC4  /* Lire Service Area Toshiba           */

/* ══════════════════════════════════════════════════════════════════════════
 *  FLAGS SECURITY STATUS (word 128 de IDENTIFY DEVICE)
 * ══════════════════════════════════════════════════════════════════════════ */

#define SEC_FLAG_SUPPORTED              (1u << 0)  /* Security Feature Set supporté  */
#define SEC_FLAG_ENABLED                (1u << 1)  /* Password activé                */
#define SEC_FLAG_LOCKED                 (1u << 2)  /* Disque verrouillé ← cible      */
#define SEC_FLAG_FROZEN                 (1u << 3)  /* État gelé par l'OS/BIOS        */
#define SEC_FLAG_COUNT_EXPIRED          (1u << 4)  /* Trop de tentatives erronées    */
#define SEC_FLAG_ENHANCED_ERASE         (1u << 5)  /* Enhanced Security Erase dispo  */

/* ══════════════════════════════════════════════════════════════════════════
 *  STRUCTURE IDENTIFY DEVICE — 512 bytes retournés par 0xEC
 *  ⚠ Les chaînes ATA (serial, model) sont word-swapped big-endian.
 *    Utiliser ata_string_fixup() pour les rendre lisibles.
 * ══════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)

typedef struct {
    uint16_t general_config;        /* word   0  : configuration générale          */
    uint16_t _pad1[9];              /* words  1-9                                   */
    char     serial_number[20];     /* words 10-19 : numéro de série (word-swap)   */
    uint16_t _pad2[3];              /* words 20-22                                  */
    char     firmware_rev[8];       /* words 23-26 : révision firmware (word-swap) */
    char     model_number[40];      /* words 27-46 : modèle disque   (word-swap)   */
    uint16_t _pad3[81];             /* words 47-127                                 */
    uint16_t security_status;       /* word  128  : flags sécurité ← CLEF          */
    uint16_t master_pwd_id;         /* word  129  : identifiant du master password  */
    uint16_t _pad4[126];            /* words 130-255                                */
} IdentifyData;

#pragma pack(pop)

/* Vérification taille à la compilation */
#ifdef __cplusplus
static_assert(sizeof(IdentifyData) == 512, "IdentifyData doit faire exactement 512 bytes");
#endif

/* ══════════════════════════════════════════════════════════════════════════
 *  STRUCTURE ATACommand — commande ATA à envoyer via ATAInterface
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t  command;       /* Opcode principal (ex: 0xEC, 0xF6, 0xE0...)  */
    uint8_t  features;      /* Registre Features / sous-commande            */
    uint8_t  sector_count;  /* Registre Sector Count                        */
    uint8_t  lba_low;       /* Registre LBA Low  (7:0)                     */
    uint8_t  lba_mid;       /* Registre LBA Mid  (15:8)                    */
    uint8_t  lba_high;      /* Registre LBA High (23:16)                   */
    uint8_t  device;        /* Registre Device (0xA0 = master, 0xB0 = slave)*/
    uint8_t  _reserved;
    uint8_t* buffer;        /* Pointeur vers le buffer de données I/O       */
    size_t   data_size;     /* Taille du buffer en bytes (0 si pas de data) */
    int      write;         /* 0 = lecture (DEV→HOST), 1 = écriture (HOST→DEV) */
} ATACommand;

/* ══════════════════════════════════════════════════════════════════════════
 *  STRUCTURE DeviceInfo — informations de base sur un disque détecté
 * ══════════════════════════════════════════════════════════════════════════ */

#define DEVICE_PATH_MAX  64
#define MODEL_MAX        42
#define SERIAL_MAX       22
#define FIRMWARE_MAX     10

typedef struct {
    char     path[DEVICE_PATH_MAX];  /* Ex: "\\\\.\\PhysicalDrive0" ou "/dev/sda" */
    char     model[MODEL_MAX];       /* Modèle nettoyé, lisible                   */
    char     serial[SERIAL_MAX];     /* Numéro de série nettoyé                   */
    char     firmware[FIRMWARE_MAX]; /* Révision firmware nettoyée                */
    uint64_t size_sectors;           /* Taille en secteurs                        */
    uint16_t security_status;        /* Copie du word 128 de IDENTIFY             */
} DeviceInfo;

/* ══════════════════════════════════════════════════════════════════════════
 *  UTILITAIRES EN LIGNE — macros et helpers portables
 * ══════════════════════════════════════════════════════════════════════════ */

/* Macro sécurisée pour tester un flag security_status */
#define SEC_IS_LOCKED(status)  (((status) & SEC_FLAG_LOCKED)  != 0)
#define SEC_IS_FROZEN(status)  (((status) & SEC_FLAG_FROZEN)  != 0)
#define SEC_IS_ENABLED(status) (((status) & SEC_FLAG_ENABLED) != 0)
#define SEC_IS_EXPIRED(status) (((status) & SEC_FLAG_COUNT_EXPIRED) != 0)

/* ══════════════════════════════════════════════════════════════════════════
 *  DÉCLARATIONS C — helpers word-swap ATA (implémentés dans ata_utils.cpp)
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * ata_string_fixup - Corrige une chaîne ATA word-swapped en place.
 *   Les chaînes ATA stockent les caractères par paires inversées :
 *   'WD' est stocké 'DW', 'C ' → ' C', etc.
 *   Cette fonction swap chaque paire d'octets et supprime le padding.
 *
 * @param str   Pointeur vers la chaîne ATA brute
 * @param len   Longueur de la chaîne (en bytes, toujours pair)
 * @param out   Buffer de sortie (len+1 bytes minimum)
 */
void ata_string_fixup(const char* str, size_t len, char* out);

/**
 * ata_security_status_str - Retourne une description lisible du security_status.
 *   Exemple : "LOCKED | ENABLED | SUPPORTED"
 *
 * @param status  Valeur du word 128 de IDENTIFY
 * @param out     Buffer de sortie (64 bytes minimum)
 */
void ata_security_status_str(uint16_t status, char* out);

#ifdef __cplusplus
} /* extern "C" */
#endif