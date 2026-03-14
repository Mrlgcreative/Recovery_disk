/*
 * main.cpp
 * --------------------------------------------------------------------------
 * Point d'entree du HDD Password Recovery Tool.
 *
 * Parse les arguments CLI puis lance la boucle interactive.
 * --------------------------------------------------------------------------
 */

#include "cli.h"
#include "config.h"

#include <cstdio>
#include <cstring>

static void print_usage(const char* prog) {
    printf("Usage : %s [options]\n\n", prog);
    printf("Options :\n");
    printf("  --dry-run         Simuler les ecritures (aucune modification disque)\n");
    printf("  --verbose         Afficher les commandes ATA en detail\n");
    printf("  --debug           Activer le log niveau DEBUG\n");
    printf("  --quiet           Minimum de sortie console\n");
    printf("  --show-password   Afficher les passwords SA en clair\n");
    printf("  --force           Continuer sans backup SA obligatoire\n");
    printf("  --uart <port>     Port serie pour Seagate F3 (ex: COM3)\n");
    printf("  --help            Afficher cette aide\n");
    printf("\n");
}

int main(int argc, char* argv[]) {
    auto& cfg = Config::get();

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dry-run") == 0)
            cfg.dry_run = true;
        else if (strcmp(argv[i], "--verbose") == 0)
            cfg.verbose = true;
        else if (strcmp(argv[i], "--debug") == 0)
            cfg.debug = true;
        else if (strcmp(argv[i], "--quiet") == 0)
            cfg.quiet = true;
        else if (strcmp(argv[i], "--show-password") == 0)
            cfg.show_pwd = true;
        else if (strcmp(argv[i], "--force") == 0)
            cfg.force = true;
        else if (strcmp(argv[i], "--uart") == 0 && i + 1 < argc)
            cfg.uart_port = argv[++i];
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        else {
            printf("Option inconnue : %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    CLI cli;
    return cli.run(argc, argv);
}
