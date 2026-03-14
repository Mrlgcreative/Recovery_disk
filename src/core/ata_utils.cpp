/*
 * ata_utils.cpp
 * ──────────────────────────────────────────────────────────────────────────
 * Fonctions utilitaires ATA portables (C pur, aucune dépendance OS).
 *   - ata_string_fixup()         : corrige le word-swap des strings ATA
 *   - ata_security_status_str()  : description lisible du word 128
 * ──────────────────────────────────────────────────────────────────────────
 */

#include "ata_defs.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ══════════════════════════════════════════════════════════════════════════
 *  ata_string_fixup
 *
 *  Les strings ATA (model, serial, firmware) sont stockées en "word-swap" :
 *  chaque paire de bytes est inversée. Exemple :
 *    Stocké en RAM : 'D','W','C',' ','D','1',...
 *    Réel          : 'W','D','C',' ','1','D',...
 *
 *  Cette fonction :
 *    1. Swap chaque paire d'octets
 *    2. Remplace les caractères non-ASCII par '?'
 *    3. Supprime le padding de fin (espaces + nuls)
 *    4. Null-termine la sortie
 * ══════════════════════════════════════════════════════════════════════════ */

void ata_string_fixup(const char* str, size_t len, char* out) {
    /* 1. Swap des paires */
    for (size_t i = 0; i + 1 < len; i += 2) {
        out[i]   = str[i + 1];
        out[i+1] = str[i];
    }
    /* Cas len impair (ne devrait pas arriver en ATA, mais sécurité) */
    if (len % 2 != 0) {
        out[len - 1] = str[len - 1];
    }

    /* 2. Sanitize — remplacer les caractères non-imprimables */
    for (size_t i = 0; i < len; ++i) {
        if (out[i] != '\0' && !isprint((unsigned char)out[i]))
            out[i] = '?';
    }

    /* 3. Supprimer le padding de fin (espaces et nuls) */
    size_t end = len;
    while (end > 0 && (out[end - 1] == ' ' || out[end - 1] == '\0'))
        --end;

    /* 4. Null-terminer */
    out[end] = '\0';
}

/* ══════════════════════════════════════════════════════════════════════════
 *  ata_security_status_str
 *
 *  Construit une chaîne lisible à partir du word 128 de IDENTIFY DEVICE.
 *  Exemple de sortie : "LOCKED | ENABLED | SUPPORTED"
 * ══════════════════════════════════════════════════════════════════════════ */

void ata_security_status_str(uint16_t status, char* out) {
    /*
     * Buffer de travail interne — dimensionné pour le pire cas :
     * tous les flags actifs = ~70 caractères.
     * On écrit d'abord ici, puis on copie dans out (max 64 bytes).
     */
    static const size_t OUT_MAX = 64;

    if (status == 0) {
        snprintf(out, OUT_MAX, "NON SUPPORTE");
        return;
    }

    out[0] = '\0';
    size_t pos = 0;

    #define APPEND_FLAG(flag, label)                                     \
        do {                                                             \
            if ((status & (flag)) && pos < OUT_MAX - 1) {                \
                int n = snprintf(out + pos, OUT_MAX - pos, "%s | ", label); \
                if (n > 0) pos += (size_t)n;                             \
            }                                                            \
        } while (0)

    APPEND_FLAG(SEC_FLAG_LOCKED,         "LOCKED");
    APPEND_FLAG(SEC_FLAG_FROZEN,         "FROZEN");
    APPEND_FLAG(SEC_FLAG_COUNT_EXPIRED,  "EXPIRED");
    APPEND_FLAG(SEC_FLAG_ENABLED,        "ENABLED");
    APPEND_FLAG(SEC_FLAG_SUPPORTED,      "SUPPORTED");
    APPEND_FLAG(SEC_FLAG_ENHANCED_ERASE, "ENHANCED_ERASE");

    #undef APPEND_FLAG

    /* Supprimer le " | " final */
    if (pos >= 3) out[pos - 3] = '\0';

    if (out[0] == '\0') snprintf(out, OUT_MAX, "OK (pas de securite active)");
}