/*
 * sa_parser.cpp — Parsing des modules Service Area (F09, F14)
 */

#include "sa_parser.h"
#include "config.h"

#include <cstring>
#include <algorithm>

/* ==========================================================================
 *  SAParser
 * ========================================================================== */

bool SAParser::is_empty_password(const uint8_t* pwd, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (pwd[i] != 0x00)
            return false;
    }
    return true;
}

std::string SAParser::password_to_string(const uint8_t* pwd, size_t len) {
    /* Chercher le premier \0 ou la fin */
    size_t end = 0;
    while (end < len && pwd[end] != 0x00) ++end;

    return std::string(reinterpret_cast<const char*>(pwd), end);
}

std::string SAParser::mask_password(const std::string& pwd, bool show) {
    if (pwd.empty()) return "(vide)";
    if (show) return pwd;

    /* Masquer : montrer le premier et dernier caractere, le reste en * */
    if (pwd.size() <= 2) return "***";
    std::string masked(pwd.size(), '*');
    masked[0] = pwd[0];
    masked[pwd.size() - 1] = pwd[pwd.size() - 1];
    return masked;
}

PasswordInfo SAParser::parse_wd_password_module(const uint8_t* data, size_t size) {
    PasswordInfo info;

    if (size < 512) return info;

    auto* mod = reinterpret_cast<const WDPasswordModule*>(data);

    /* Verifier le module_id (peu fiable mais indicatif) */
    if (mod->module_id != 0x0020 && mod->module_id != 0x2000) {
        /* On tente quand meme — certains firmware ne mettent pas le bon ID */
    }

    info.found = true;
    info.security_flags = mod->security_flags;

    /* Extraire les passwords */
    info.user_password   = password_to_string(mod->user_password, 32);
    info.master_password = password_to_string(mod->master_password, 32);

    info.is_empty_password =
        is_empty_password(mod->user_password, 32) &&
        is_empty_password(mod->master_password, 32);

    return info;
}
