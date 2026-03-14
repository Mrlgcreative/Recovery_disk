/*
 * device.cpp — Device + gestion FROZEN (F03, F15)
 */

#include "device.h"

#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#endif

/* ==========================================================================
 *  attempt_unfreeze — F15 : Contournement FROZEN
 * ========================================================================== */

bool Device::attempt_unfreeze() {
    if (!is_frozen()) return true;   /* Deja non-gele */

#ifdef _WIN32
    /*
     * Windows : impossible de faire un suspend programmatique simple.
     * On affiche un message au technicien pour qu'il fasse l'operation
     * manuellement depuis le Gestionnaire d'energie.
     */
    printf("\n");
    printf("  ================================================================\n");
    printf("  Le disque est FROZEN (gele par le BIOS/OS).\n");
    printf("  \n");
    printf("  Pour degeler, effectuez un Suspend-to-RAM (mise en veille)\n");
    printf("  depuis le Gestionnaire d'alimentation de Windows, puis\n");
    printf("  reveilllez le PC.\n");
    printf("  \n");
    printf("  Alternative : deconnectez le cable SATA du disque pendant\n");
    printf("  que le PC est allume, puis reconnectez-le (hot-swap).\n");
    printf("  \n");
    printf("  Appuyez sur Entree une fois le disque degele...\n");
    printf("  ================================================================\n");

    /* Attendre que l'utilisateur ait fait l'operation */
    getchar();

#else
    /*
     * Linux : tenter un sleep/wake cycle via /sys/power/state.
     * Necessite les droits root. Si ca echoue, afficher le message.
     */
    printf("  Tentative de suspend-to-RAM pour degeler le disque...\n");

    /* Verifier qu'on est root */
    if (geteuid() != 0) {
        printf("  ERREUR : droits root requis pour le suspend-to-RAM.\n");
        printf("  Executez manuellement : echo mem > /sys/power/state\n");
        printf("  Puis relancez le programme.\n");
        return false;
    }

    FILE* f = fopen("/sys/power/state", "w");
    if (f) {
        fprintf(f, "mem\n");
        fclose(f);
        /* Apres le reveil, attendre un peu que le disque se reinitialise */
        sleep(2);
    } else {
        printf("  Impossible d'ecrire dans /sys/power/state.\n");
        printf("  Executez manuellement : echo mem > /sys/power/state\n");
        return false;
    }
#endif

    /* Rafraichir IDENTIFY pour verifier que FROZEN est parti */
    if (!refresh_identify()) {
        printf("  ERREUR : impossible de relire IDENTIFY apres unfreeze.\n");
        return false;
    }

    if (is_frozen()) {
        printf("  Le disque est toujours FROZEN apres la tentative.\n");
        return false;
    }

    printf("  Disque degele avec succes.\n");
    return true;
}
