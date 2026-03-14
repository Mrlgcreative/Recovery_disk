/*
 * test_sa_parser.cpp — Tests SA Parser (F18)
 */

#include <catch2/catch_test_macros.hpp>

#include "sa_parser.h"

#include <cstring>

TEST_CASE("WDPasswordModule est 512 bytes", "[sa_parser]") {
    REQUIRE(sizeof(WDPasswordModule) == 512);
}

TEST_CASE("is_empty_password detecte buffer vide", "[sa_parser]") {
    uint8_t empty[32]{};
    REQUIRE(SAParser::is_empty_password(empty, 32) == true);

    uint8_t notempty[32]{};
    notempty[5] = 0x41;  /* 'A' */
    REQUIRE(SAParser::is_empty_password(notempty, 32) == false);
}

TEST_CASE("password_to_string arrete au premier null", "[sa_parser]") {
    uint8_t pwd[32]{};
    pwd[0] = 'H'; pwd[1] = 'e'; pwd[2] = 'l'; pwd[3] = 'l'; pwd[4] = 'o';
    auto s = SAParser::password_to_string(pwd, 32);
    REQUIRE(s == "Hello");
}

TEST_CASE("mask_password masque correctement", "[sa_parser]") {
    REQUIRE(SAParser::mask_password("", false) == "(vide)");
    REQUIRE(SAParser::mask_password("Hello", true) == "Hello");
    REQUIRE(SAParser::mask_password("AB", false) == "***");

    auto masked = SAParser::mask_password("Hello", false);
    REQUIRE(masked[0] == 'H');
    REQUIRE(masked[4] == 'o');
    REQUIRE(masked[1] == '*');
}

TEST_CASE("parse_wd_password_module avec passwords", "[sa_parser]") {
    uint8_t buf[512]{};
    auto* mod = reinterpret_cast<WDPasswordModule*>(buf);
    mod->module_id = 0x0020;

    /* Ecrire un password user */
    const char* user_pwd = "secret123";
    memcpy(mod->user_password, user_pwd, strlen(user_pwd));

    /* Ecrire un password master */
    const char* master_pwd = "master456";
    memcpy(mod->master_password, master_pwd, strlen(master_pwd));

    mod->security_flags = 0x03;   /* locked + enabled */

    auto info = SAParser::parse_wd_password_module(buf, 512);
    REQUIRE(info.found == true);
    REQUIRE(info.user_password == "secret123");
    REQUIRE(info.master_password == "master456");
    REQUIRE(info.security_flags == 0x03);
    REQUIRE(info.is_empty_password == false);
}

TEST_CASE("parse_wd_password_module avec passwords vides", "[sa_parser]") {
    uint8_t buf[512]{};
    auto* mod = reinterpret_cast<WDPasswordModule*>(buf);
    mod->module_id = 0x0020;
    /* Tout a zero — passwords vides */

    auto info = SAParser::parse_wd_password_module(buf, 512);
    REQUIRE(info.found == true);
    REQUIRE(info.is_empty_password == true);
    REQUIRE(info.user_password.empty() == true);
}

TEST_CASE("parse_wd_password_module buffer trop petit", "[sa_parser]") {
    uint8_t buf[256]{};
    auto info = SAParser::parse_wd_password_module(buf, 256);
    REQUIRE(info.found == false);
}
