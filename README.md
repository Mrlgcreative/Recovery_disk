# HDD Password Recovery Tool

Outil bas-niveau C++20 pour le diagnostic et la récupération de mots de passe ATA sur disques durs verrouillés (HDD Security Feature Set).

## Prérequis

| Plateforme              | Compilateur minimum     | Droits                |
| ----------------------- | ----------------------- | --------------------- |
| Windows 10+ (64-bit)    | MSVC 19.29+ / MinGW-w64 | Administrateur        |
| Linux (kernel ≥ 2.6.30) | GCC 10+ / Clang 12+     | root ou groupe `disk` |

**CMake 3.20+** requis.

## Compilation

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Structure du projet

```
include/            Headers publics (ata_defs.h, ata_interface.h)
src/core/           Logique métier portable (factory, utilitaires ATA)
src/os/             Implémentations OS-spécifiques (Windows / Linux)
tests/              Tests unitaires
```

## Architecture

```
ATAInterface (abstraite)
├── WinATAInterface   → DeviceIoControl / ATA_PASS_THROUGH_EX
└── LinuxATAInterface → ioctl SG_IO / SAT PASS-THROUGH(16)
```

## Vendeurs supportés (planifié)

- Western Digital (VSC 0xE0/0xE1)
- Seagate (SMART vendor 0xD6)
- Toshiba / HGST (0xC0/0xC1)

## Licence

Usage privé uniquement — outil de diagnostic.
