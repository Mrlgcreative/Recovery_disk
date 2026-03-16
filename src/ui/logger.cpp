/*
 * logger.cpp — Logger spdlog double sink (F12)
 */

#include "logger.h"
#include "config.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <cstdio>
#include <ctime>
#include <vector>

static std::shared_ptr<spdlog::logger> g_logger;

/* ==========================================================================
 *  Logger
 * ========================================================================== */

void Logger::init(bool verbose, bool debug, bool quiet) {
    /* Generer le nom du fichier log avec horodatage */
    char logfile[128];
    time_t now = time(nullptr);
    struct tm t;
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    snprintf(logfile, sizeof(logfile), "hdd_unlock_%04d%02d%02d_%02d%02d%02d.log",
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);

    /* Creer les sinks */
    std::vector<spdlog::sink_ptr> sinks;

    /* Console sink avec couleurs */
    if (!quiet) {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_pattern("[%H:%M:%S] [%^%l%$] %v");

        if (debug)
            console_sink->set_level(spdlog::level::debug);
        else if (verbose)
            console_sink->set_level(spdlog::level::info);
        else
            console_sink->set_level(spdlog::level::warn);

        sinks.push_back(console_sink);
    }

    /* Fichier sink — toujours tout logger */
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logfile, true);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    file_sink->set_level(spdlog::level::trace);
    sinks.push_back(file_sink);

    /* Creer le logger multi-sink */
    g_logger = std::make_shared<spdlog::logger>("hdd_unlock", sinks.begin(), sinks.end());
    g_logger->set_level(spdlog::level::trace);   /* Le filtrage est fait par les sinks */
    spdlog::set_default_logger(g_logger);

    g_logger->info("Logger initialise (fichier: {})", logfile);
}

std::shared_ptr<spdlog::logger>& Logger::get() {
    return g_logger;
}

void Logger::log_ata_command(const ATACommand& cmd, const char* result) {
    if (!g_logger) return;

    g_logger->info("ATA CMD: opcode=0x{:02X} feat=0x{:02X} sc={} "
                   "lba={:02X}/{:02X}/{:02X} dev=0x{:02X} "
                   "data_size={} write={} -> {}",
                   cmd.command, cmd.features, cmd.sector_count,
                   cmd.lba_low, cmd.lba_mid, cmd.lba_high, cmd.device,
                   cmd.data_size, cmd.write, result);
}

void Logger::log_hex_dump(const uint8_t* data, size_t size, const char* label) {
    if (!g_logger) return;

    g_logger->debug("--- HEX DUMP: {} ({} bytes) ---", label, size);

    std::string line;
    for (size_t i = 0; i < size; ++i) {
        char hex[4];
        snprintf(hex, sizeof(hex), "%02X ", data[i]);
        line += hex;
        if ((i + 1) % 16 == 0) {
            g_logger->debug("  {:04X}: {}", i - 15, line);
            line.clear();
        }
    }
    if (!line.empty()) {
        g_logger->debug("  {:04X}: {}", (size / 16) * 16, line);
    }
}

void Logger::shutdown() {
    if (g_logger) {
        g_logger->flush();
    }
    spdlog::shutdown();
}
