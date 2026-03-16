#pragma once
/*
 * logger.h — Logger thread-safe double sink : console + fichier (F12)
 *
 * Utilise spdlog via FetchContent.
 * Console : couleurs ANSI (vert=INFO, jaune=WARN, rouge=ERROR, cyan=DEBUG)
 * Fichier : hdd_unlock_YYYYMMDD_HHMMSS.log — toutes les commandes ATA
 */

#include "ata_defs.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <string>
#include <memory>

namespace Logger {

    /* Initialiser le logger (appeler une fois au demarrage) */
    void init(bool verbose, bool debug, bool quiet);

    /* Logger principal */
    std::shared_ptr<spdlog::logger>& get();

    /* Log d'une commande ATA (opcode, registres, resultat) */
    void log_ata_command(const ATACommand& cmd, const char* result);

    /* Log hex dump d'un buffer */
    void log_hex_dump(const uint8_t* data, size_t size, const char* label);

    /* Shutdown propre */
    void shutdown();
}
