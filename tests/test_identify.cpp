/*
 * test_identify.cpp — Tests IDENTIFY DEVICE parsing (F18)
 */

#include <catch2/catch_test_macros.hpp>

#include "ata_defs.h"

#include <cstring>
#include <cstdint>

TEST_CASE("IdentifyData est exactement 512 bytes", "[identify]") {
    REQUIRE(sizeof(IdentifyData) == 512);
}

TEST_CASE("IdentifyData offsets critiques", "[identify]") {
    IdentifyData id{};
    const uintptr_t base       = reinterpret_cast<uintptr_t>(&id);
    const uintptr_t serial_off = reinterpret_cast<uintptr_t>(&id.serial_number) - base;
    const uintptr_t model_off  = reinterpret_cast<uintptr_t>(&id.model_number)  - base;
    const uintptr_t sec_off    = reinterpret_cast<uintptr_t>(&id.security_status) - base;

    REQUIRE(serial_off == 20);    /* words 10-19 : 10*2=20 */
    REQUIRE(model_off  == 54);    /* words 27-46 : 27*2=54 */
    REQUIRE(sec_off    == 256);   /* word 128    : 128*2=256 */
}

TEST_CASE("ata_string_fixup word-swap WD", "[identify]") {
    /* Simule un model WD word-swapped */
    const char raw[] = {
        'D','W',' ','C','D','W','0','1','Z','E','X','E','0','-','B','0',
        '5','N','0','A',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' '
    };
    char out[41]{};
    ata_string_fixup(raw, 40, out);

    REQUIRE(std::string(out).starts_with("WDC "));
}

TEST_CASE("ata_string_fixup word-swap Seagate", "[identify]") {
    const char raw[] = {
        'T','S','0','2','0','0','D','M','0','0','8','-','R','2','1','F',
        '0','0',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' '
    };
    char out[41]{};
    ata_string_fixup(raw, 40, out);

    REQUIRE(std::string(out).starts_with("ST"));
}

TEST_CASE("Security status flags", "[identify]") {
    REQUIRE(SEC_FLAG_SUPPORTED      == (1u << 0));
    REQUIRE(SEC_FLAG_ENABLED        == (1u << 1));
    REQUIRE(SEC_FLAG_LOCKED         == (1u << 2));
    REQUIRE(SEC_FLAG_FROZEN         == (1u << 3));
    REQUIRE(SEC_FLAG_COUNT_EXPIRED  == (1u << 4));
    REQUIRE(SEC_FLAG_ENHANCED_ERASE == (1u << 5));
}

TEST_CASE("SEC_IS macros", "[identify]") {
    REQUIRE(SEC_IS_LOCKED(0x0007) == 1);
    REQUIRE(SEC_IS_FROZEN(0x0008) == 1);
    REQUIRE(SEC_IS_LOCKED(0x0000) == 0);
    REQUIRE(SEC_IS_ENABLED(0x0002) == 1);
    REQUIRE(SEC_IS_EXPIRED(0x0010) == 1);
}

TEST_CASE("ata_security_status_str contient les bons flags", "[identify]") {
    char buf[64]{};

    ata_security_status_str(SEC_FLAG_LOCKED | SEC_FLAG_ENABLED | SEC_FLAG_SUPPORTED, buf);
    REQUIRE(std::string(buf).find("LOCKED") != std::string::npos);
    REQUIRE(std::string(buf).find("ENABLED") != std::string::npos);
    REQUIRE(std::string(buf).find("SUPPORTED") != std::string::npos);

    ata_security_status_str(SEC_FLAG_FROZEN, buf);
    REQUIRE(std::string(buf).find("FROZEN") != std::string::npos);

    ata_security_status_str(0, buf);
    REQUIRE(std::string(buf).find("NON") != std::string::npos);
}
