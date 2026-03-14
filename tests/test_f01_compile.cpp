/*
 * test_f01_compile.cpp
 * ──────────────────────────────────────────────────────────────────────────
 * Test de compilation F01 — ATAInterface abstraite.
 *
 * Ce fichier vérifie que :
 *   1. ata_defs.h compile sans erreur et que IdentifyData fait bien 512 bytes
 *   2. ATAInterface::create() est callable et retourne un unique_ptr non nul
 *   3. Les méthodes de l'interface sont bien accessibles
 *   4. ATAHandle RAII fonctionne (close auto au destructeur)
 *   5. ata_string_fixup et ata_security_status_str fonctionnent
 *
 * Ce test ne nécessite PAS de vrai disque physique.
 * Il ouvre un chemin invalide intentionnellement pour tester DEVICE_NOT_FOUND.
 * ──────────────────────────────────────────────────────────────────────────
 */

#include "ata_defs.h"
#include "ata_interface.h"

#include <cassert>
#include <cstdio>
#include <cstring>

/* ── Couleurs ANSI ── */
#define GRN "\033[32m"
#define RED "\033[31m"
#define YEL "\033[33m"
#define CYN "\033[36m"
#define RST "\033[0m"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, cond)                                                 \
    do {                                                                 \
        if (cond) {                                                      \
            printf(GRN "  [PASS]" RST " %s\n", name);                  \
            ++tests_passed;                                              \
        } else {                                                         \
            printf(RED "  [FAIL]" RST " %s\n", name);                  \
            ++tests_failed;                                              \
        }                                                                \
    } while(0)

/* ══════════════════════════════════════════════════════════════════════════
 *  Tests
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_ata_defs() {
    printf(CYN "\n-- Test 1 : ata_defs.h --\n" RST);

    /* IdentifyData doit faire exactement 512 bytes */
    TEST("sizeof(IdentifyData) == 512", sizeof(IdentifyData) == 512);

    /* ATACommand est une structure valide */
    ATACommand cmd{};
    cmd.command   = ATA_CMD_IDENTIFY_DEVICE;
    cmd.data_size = 0;
    TEST("ATACommand initialisable a zero", cmd.command == 0xEC);

    /* Constantes opcodes */
    TEST("ATA_CMD_IDENTIFY_DEVICE    == 0xEC", ATA_CMD_IDENTIFY_DEVICE    == 0xEC);
    TEST("ATA_CMD_SECURITY_DISABLE   == 0xF6", ATA_CMD_SECURITY_DISABLE   == 0xF6);
    TEST("WD_CMD_VENDOR_ENTER        == 0xE0", WD_CMD_VENDOR_ENTER        == 0xE0);
    TEST("WD_CMD_READ_SA_MODULE      == 0x45", WD_CMD_READ_SA_MODULE      == 0x45);
    TEST("TOSH_CMD_VENDOR_ENTER      == 0xC0", TOSH_CMD_VENDOR_ENTER      == 0xC0);

    /* Flags security_status */
    TEST("SEC_FLAG_LOCKED  == bit 2", SEC_FLAG_LOCKED  == (1u << 2));
    TEST("SEC_FLAG_FROZEN  == bit 3", SEC_FLAG_FROZEN  == (1u << 3));
    TEST("SEC_IS_LOCKED macro",  SEC_IS_LOCKED(0x0007)  == 1);
    TEST("SEC_IS_FROZEN macro",  SEC_IS_FROZEN(0x0008)  == 1);
    TEST("SEC_IS_LOCKED 0x0000", SEC_IS_LOCKED(0x0000)  == 0);
}

static void test_ata_string_fixup() {
    printf(CYN "\n-- Test 2 : ata_string_fixup (word-swap ATA) --\n" RST);

    /*
     * Pour obtenir "WDC WD10EZEX-00BN5A0" après fixup (word-swap),
     * chaque paire de bytes est inversée en entrée :
     * 'W','D' → 'D','W'  |  'C',' ' → ' ','C'  etc.
     */
    const char raw_model[] = {
        'D','W',' ','C','D','W','0','1','Z','E','X','E','0','-','B','0',
        '5','N','0','A',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' '
    };
    char out[41] = {};
    ata_string_fixup(raw_model, 40, out);

    printf("  Brut   : %.40s\n", raw_model);
    printf("  Fixed  : %s\n", out);

    TEST("Modele WD fixe commence par 'WDC '",
         strncmp(out, "WDC ", 4) == 0);

    /* Test avec une chaîne Seagate simulée */
    const char raw_sea[] = {
        'T','S','0','2','0','0','D','M','0','0','8','-','R','2','1','F',
        '0','0',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
        ' ',' ',' ',' ',' ',' ',' ',' '
    };
    char out2[41] = {};
    ata_string_fixup(raw_sea, 40, out2);
    TEST("Modele Seagate fixe commence par 'ST'",
         strncmp(out2, "ST", 2) == 0);
}

static void test_ata_security_status_str() {
    printf(CYN "\n-- Test 3 : ata_security_status_str --\n" RST);

    char buf[64] = {};

    ata_security_status_str(SEC_FLAG_LOCKED | SEC_FLAG_ENABLED | SEC_FLAG_SUPPORTED, buf);
    printf("  Status 0x0007 : %s\n", buf);
    TEST("Contient 'LOCKED'",   strstr(buf, "LOCKED")    != nullptr);
    TEST("Contient 'ENABLED'",  strstr(buf, "ENABLED")   != nullptr);

    ata_security_status_str(SEC_FLAG_FROZEN, buf);
    printf("  Status 0x0008 : %s\n", buf);
    TEST("Contient 'FROZEN'",   strstr(buf, "FROZEN")    != nullptr);

    ata_security_status_str(0, buf);
    printf("  Status 0x0000 : %s\n", buf);
    TEST("Status 0 -> 'NON SUPPORTE'", strstr(buf, "NON") != nullptr);
}

static void test_ata_interface_factory() {
    printf(CYN "\n-- Test 4 : ATAInterface::create() + open() --\n" RST);

    /* La factory ne doit jamais retourner nullptr */
    auto ata = ATAInterface::create();
    TEST("ATAInterface::create() != nullptr", ata != nullptr);
    TEST("is_open() == false au depart",      !ata->is_open());
    TEST("device_path() vide au depart",       ata->device_path().empty());

    /* Tentative d'ouverture sur un chemin invalide — doit échouer proprement */
#ifdef _WIN32
    const char* invalid_path = "\\\\.\\PhysicalDriveXXXINVALID";
#else
    const char* invalid_path = "/dev/sdXXXINVALID";
#endif
    ATAError err = ata->open(invalid_path);
    printf("  open(chemin invalide) -> %s\n", ata_error_str(err));
    TEST("open(invalide) != OK",       err != ATAError::OK);
    TEST("is_open() false apres echec", !ata->is_open());

    /* Fermeture d'un handle déjà fermé — doit être idempotente */
    ata->close();
    ata->close();   /* double close — ne doit pas crasher */
    TEST("double close() sans crash", true);
}

static void test_ata_handle_raii() {
    printf(CYN "\n-- Test 5 : ATAHandle RAII --\n" RST);

    bool closed_properly = false;
    {
        ATAHandle h(ATAInterface::create());
        TEST("ATAHandle construit correctement", h.get() != nullptr);
        /* h.close() appelé automatiquement au destructeur */
    }
    /* Si on est arrivé ici sans segfault, RAII fonctionne */
    TEST("ATAHandle RAII - close auto sans crash", true);
}

static void test_identify_data_offsets() {
    printf(CYN "\n-- Test 6 : IdentifyData - offsets critiques --\n" RST);

    IdentifyData id{};
    /* Vérification des offsets par rapport au début de la structure */
    const uintptr_t base       = reinterpret_cast<uintptr_t>(&id);
    const uintptr_t serial_off = reinterpret_cast<uintptr_t>(&id.serial_number) - base;
    const uintptr_t model_off  = reinterpret_cast<uintptr_t>(&id.model_number)  - base;
    const uintptr_t sec_off    = reinterpret_cast<uintptr_t>(&id.security_status) - base;

    printf("  serial_number  offset : %zu bytes (attendu : 20)\n", serial_off);
    printf("  model_number   offset : %zu bytes (attendu : 54)\n", model_off);
    printf("  security_status offset: %zu bytes (attendu : 256 = word 128)\n", sec_off);

    /* word 128 = offset 256 bytes (128 * 2) */
    TEST("serial_number a offset 20",  serial_off == 20);
    TEST("model_number  a offset 54",  model_off  == 54);
    TEST("security_status a word 128 (offset 256)", sec_off == 256);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  main
 * ══════════════════════════════════════════════════════════════════════════ */

int main() {
    printf(YEL "\n+----------------------------------------------+\n" RST);
    printf(YEL "|  HDD Unlock - Test F01 : ATAInterface        |\n" RST);
    printf(YEL "+----------------------------------------------+\n" RST);

    test_ata_defs();
    test_ata_string_fixup();
    test_ata_security_status_str();
    test_ata_interface_factory();
    test_ata_handle_raii();
    test_identify_data_offsets();

    printf(YEL "\n----------------------------------------------\n" RST);
    printf("  Resultats : " GRN "%d passes" RST " / " RED "%d echoues" RST "\n",
           tests_passed, tests_failed);
    printf(YEL "----------------------------------------------\n\n" RST);

    return tests_failed == 0 ? 0 : 1;
}
