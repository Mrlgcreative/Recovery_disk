/*
 * test_ram_patcher.cpp — Tests RAMPatcher + UnlockResult (F18)
 */

#include <catch2/catch_test_macros.hpp>

#include "ram_patcher.h"

TEST_CASE("unlock_result_str retourne des descriptions valides", "[ram_patcher]") {
    REQUIRE(std::string(unlock_result_str(UnlockResult::SUCCESS)).find("SUCCES") != std::string::npos);
    REQUIRE(std::string(unlock_result_str(UnlockResult::VENDOR_NOT_SUPPORTED)).find("non supporte") != std::string::npos);
    REQUIRE(std::string(unlock_result_str(UnlockResult::VENDOR_MODE_FAIL)).find("vendor") != std::string::npos);
    REQUIRE(std::string(unlock_result_str(UnlockResult::RAM_PATCH_FAIL)).find("RAM") != std::string::npos);
    REQUIRE(std::string(unlock_result_str(UnlockResult::ATA_CMD_FAIL)).find("ATA") != std::string::npos);
    REQUIRE(std::string(unlock_result_str(UnlockResult::STILL_LOCKED)).find("verrouille") != std::string::npos);
    REQUIRE(std::string(unlock_result_str(UnlockResult::FROZEN)).find("gele") != std::string::npos);
    REQUIRE(std::string(unlock_result_str(UnlockResult::EXPIRED)).find("expire") != std::string::npos);
    REQUIRE(std::string(unlock_result_str(UnlockResult::DRY_RUN)).find("dry-run") != std::string::npos);
}

TEST_CASE("UnlockResult enum couvre tous les cas", "[ram_patcher]") {
    /* Verification que chaque valeur de l'enum a une description non vide */
    auto check = [](UnlockResult r) {
        const char* s = unlock_result_str(r);
        return s != nullptr && s[0] != '\0';
    };

    REQUIRE(check(UnlockResult::SUCCESS));
    REQUIRE(check(UnlockResult::VENDOR_NOT_SUPPORTED));
    REQUIRE(check(UnlockResult::VENDOR_MODE_FAIL));
    REQUIRE(check(UnlockResult::RAM_PATCH_FAIL));
    REQUIRE(check(UnlockResult::ATA_CMD_FAIL));
    REQUIRE(check(UnlockResult::STILL_LOCKED));
    REQUIRE(check(UnlockResult::FROZEN));
    REQUIRE(check(UnlockResult::EXPIRED));
    REQUIRE(check(UnlockResult::BACKUP_FAIL));
    REQUIRE(check(UnlockResult::PASSWORD_UNLOCK));
    REQUIRE(check(UnlockResult::DRY_RUN));
}
