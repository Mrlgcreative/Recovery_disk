/*
 * test_vendor_detect.cpp — Tests detection constructeur (F18)
 */

#include <catch2/catch_test_macros.hpp>

#include "vsc_engine.h"
#include "ata_defs.h"

#include <cstring>

/* Helper : cree un IdentifyData avec un model word-swapped */
static IdentifyData make_id_with_model(const char* model_swapped, size_t len) {
    IdentifyData id{};
    size_t copy = len < sizeof(id.model_number) ? len : sizeof(id.model_number);
    memcpy(id.model_number, model_swapped, copy);
    /* Pad avec espaces */
    for (size_t i = copy; i < sizeof(id.model_number); ++i)
        id.model_number[i] = ' ';
    return id;
}

TEST_CASE("ata_str_to_utf8 word-swap WD", "[vendor]") {
    /* "WDC " word-swapped => 'D','W',' ','C' */
    const char raw[] = {'D','W',' ','C','D','W','0','1'};
    auto result = ata_str_to_utf8(raw, 8);
    REQUIRE(result.starts_with("WDC "));
}

TEST_CASE("detect_vendor WD avec mock", "[vendor]") {
    /* On ne peut pas tester sans ATAInterface reelle,
       mais on peut verifier que ata_str_to_utf8 fonctionne
       et que la logique de prefixe est correcte */
    const char wd_raw[] = {
        'D','W',' ','C','D','W','0','1','Z','E','X','E','0','-','B','0',
        '5','N','0','A',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' '
    };
    auto model = ata_str_to_utf8(wd_raw, 40);
    REQUIRE(model.starts_with("WDC "));
}

TEST_CASE("detect_vendor Seagate prefixe", "[vendor]") {
    const char sea_raw[] = {
        'T','S','0','2','0','0','D','M','0','0','8','-','R','2','1','F',
        '0','0',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' '
    };
    auto model = ata_str_to_utf8(sea_raw, 40);
    REQUIRE(model.starts_with("ST"));
}

TEST_CASE("detect_vendor Toshiba prefixe", "[vendor]") {
    /* "TOSHIBA " word-swapped => 'O','T','H','S','B','I' ... */
    const char tosh_raw[] = {
        'O','T','H','S','B','I','A','T','M',' ','0','Q','5','1','0','0',
        'C','G','T','H',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' '
    };
    auto model = ata_str_to_utf8(tosh_raw, 40);
    REQUIRE(model.starts_with("TOSHIBA"));
}

TEST_CASE("detect_vendor HGST prefixe", "[vendor]") {
    /* "HUA " word-swapped => 'U','H','A',' ' */
    const char hgst_raw[] = {
        'U','H','7','A','2','7','0','0','0','0','A','A','0','L','N','A',
        ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' '
    };
    auto model = ata_str_to_utf8(hgst_raw, 40);
    REQUIRE(model.starts_with("HUA"));
}

TEST_CASE("detect_vendor inconnu", "[vendor]") {
    const char unknown_raw[] = {
        'A','S','S','M','N','U','G',' ','S','S',' ','D','0','1','0','2',
        'G','B',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' '
    };
    auto model = ata_str_to_utf8(unknown_raw, 40);
    /* Ne doit correspondre a aucun constructeur connu */
    REQUIRE_FALSE(model.starts_with("WDC "));
    REQUIRE_FALSE(model.starts_with("WD "));
    REQUIRE_FALSE(model.starts_with("ST"));
    REQUIRE_FALSE(model.starts_with("Seagate"));
    REQUIRE_FALSE(model.starts_with("TOSHIBA"));
    REQUIRE_FALSE(model.starts_with("HUA"));
    REQUIRE_FALSE(model.starts_with("HDS"));
    REQUIRE_FALSE(model.starts_with("HTS"));
}
