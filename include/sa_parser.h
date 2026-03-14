#pragma once
/*
 * sa_parser.h — Structures et parsing Service Area (F09, F14)
 */

#include <cstdint>
#include <cstddef>
#include <string>
#include <optional>

/* ==========================================================================
 *  WDPasswordModule — Module 0x20 Western Digital (512 bytes)
 * ========================================================================== */

#pragma pack(push, 1)

struct WDPasswordModule {
    uint8_t  header[4];            /* 0x00 : magic bytes                       */
    uint16_t module_id;            /* 0x04 : doit etre 0x0020                  */
    uint8_t  _pad1[20];           /* 0x06                                     */
    uint8_t  user_password[32];    /* 0x1A : USER PASSWORD (null-terminated)   */
    uint8_t  _pad2[8];            /* 0x3A                                     */
    uint8_t  master_password[32];  /* 0x42 : MASTER PASSWORD (null-terminated) */
    uint8_t  security_flags;       /* 0x62 : bit0=locked, bit1=enabled         */
    uint8_t  _pad3[413];          /* 0x63 → total 512 bytes                   */
};

#pragma pack(pop)

static_assert(sizeof(WDPasswordModule) == 512, "WDPasswordModule doit faire 512 bytes");

/* ==========================================================================
 *  PasswordInfo — Resultat de l'extraction de mots de passe
 * ========================================================================== */

struct PasswordInfo {
    bool found = false;
    std::string user_password;
    std::string master_password;
    uint8_t security_flags = 0;
    bool is_empty_password = false;   /* les deux a 0x00 = password vide */
};

/* ==========================================================================
 *  SAParser — Fonctions de parsing des modules SA
 * ========================================================================== */

namespace SAParser {
    /* Parse un module WD 0x20 brut (512 bytes) */
    PasswordInfo parse_wd_password_module(const uint8_t* data, size_t size);

    /* Determine si un buffer de password est tout a zero */
    bool is_empty_password(const uint8_t* pwd, size_t len);

    /* Convertit un buffer de password en string (stop au premier \0 ou len) */
    std::string password_to_string(const uint8_t* pwd, size_t len);

    /* Masque un password pour l'affichage (*** sauf si --show-password) */
    std::string mask_password(const std::string& pwd, bool show);
}
