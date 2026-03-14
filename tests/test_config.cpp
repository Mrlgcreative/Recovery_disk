/*
 * test_config.cpp — Tests Config singleton + dry-run (F18)
 */

#include <catch2/catch_test_macros.hpp>

#include "config.h"

TEST_CASE("Config singleton retourne la meme instance", "[config]") {
    auto& c1 = Config::get();
    auto& c2 = Config::get();
    REQUIRE(&c1 == &c2);
}

TEST_CASE("Config dry_run est false par defaut", "[config]") {
    REQUIRE(Config::get().dry_run == false);
}

TEST_CASE("is_write_command identifie les commandes d'ecriture", "[config]") {
    /* Commandes d'ecriture */
    REQUIRE(Config::is_write_command(0xF1) == true);   /* SECURITY SET PASSWORD */
    REQUIRE(Config::is_write_command(0xF2) == true);   /* SECURITY UNLOCK */
    REQUIRE(Config::is_write_command(0xF6) == true);   /* SECURITY DISABLE */
    REQUIRE(Config::is_write_command(0x46) == true);   /* WD WRITE RAM */
    REQUIRE(Config::is_write_command(0xE0) == true);   /* WD VENDOR ENTER */
    REQUIRE(Config::is_write_command(0xC0) == true);   /* TOSHIBA VENDOR ENTER */

    /* Commandes de lecture */
    REQUIRE(Config::is_write_command(0xEC) == false);  /* IDENTIFY DEVICE */
    REQUIRE(Config::is_write_command(0x45) == false);  /* WD READ SA */
    REQUIRE(Config::is_write_command(0xC4) == false);  /* TOSHIBA READ SA */
    REQUIRE(Config::is_write_command(0xB0) == false);  /* SMART */
}
