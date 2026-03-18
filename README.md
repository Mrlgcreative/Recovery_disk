# HDD Password Recovery Tool

<p align="center">
  <img src="resources/logo_256.png" alt="HDD Password Recovery Tool" width="128">
</p>

<p align="center">
  <strong>Outil de diagnostic et récupération de mots de passe ATA</strong><br>
  C++20 · Dear ImGui · DirectX 11 · Windows
</p>

---

Outil bas-niveau C++20 pour le diagnostic et la récupération de mots de passe ATA sur disques durs verrouillés (HDD Security Feature Set). Interface graphique complète avec docking layout, thème clair/sombre, et outils de diagnostic avancés.

## Fonctionnalités

- **Scan automatique** des disques physiques connectés
- **IDENTIFY DEVICE** — lecture des données d'identification ATA brutes
- **Security Status** — affichage coloré des flags de sécurité (Locked, Frozen, Enabled…)
- **Hex Viewer** — dump 512 octets du secteur IDENTIFY
- **Détection constructeur** — identification automatique via commandes VSC
- **Déverrouillage** — workflow RAM patch / envoi de mot de passe ATA
- **Backup Service Area** — sauvegarde des modules SA avec hash SHA-256
- **Extraction de mots de passe** — lecture des mots de passe stockés dans la SA
- **Console intégrée** — journal horodaté des événements
- **Thème clair / sombre** — basculable depuis le menu
- **Police Montserrat** — rendu typographique soigné

## Captures d'écran

L'interface utilise un docking layout avec 9 panneaux :

| Panneau | Description |
|---|---|
| Disques | Liste cliquable des disques détectés |
| Détails | Informations IDENTIFY du disque sélectionné |
| Sécurité ATA | Flags de sécurité avec code couleur |
| Hex Viewer | Dump hexadécimal brut (512 bytes) |
| Constructeur | Détection vendor (WD, Seagate, Toshiba, HGST) |
| Déverrouillage | Workflow de déverrouillage pas-à-pas |
| Backup SA | Sauvegarde Service Area avec intégrité SHA-256 |
| Mots de passe | Extraction et affichage des passwords |
| Console | Journal des événements en temps réel |

## Prérequis

| Plateforme | Compilateur | Droits |
|---|---|---|
| Windows 10+ (64-bit) | MSVC 19.29+ / MinGW-w64 (GCC 13+) | Administrateur |
| Linux (kernel ≥ 2.6.30) | GCC 10+ / Clang 12+ | root ou groupe `disk` |

- **CMake 3.20+**
- **DirectX 11** (inclus dans Windows SDK)
- Les dépendances (Dear ImGui, spdlog, Catch2) sont téléchargées automatiquement via FetchContent

## Compilation

```bash
# Configuration
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build complet (CLI + GUI + tests)
cmake --build build

# GUI seul
cmake --build build --target hdd_unlock_gui

# CLI seul
cmake --build build --target hdd_unlock

# Tests
cmake --build build --target test_all
./build/test_all
```

Avec **Make** (MinGW) :

```bash
cd build
cmake ..
make hdd_unlock_gui    # Interface graphique
make hdd_unlock        # Ligne de commande
make test_all          # Tests unitaires
```

## Structure du projet

```
├── include/                Headers publics
│   ├── ata_defs.h          Constantes ATA Security Feature Set
│   ├── ata_interface.h     Interface abstraite ATA (factory)
│   ├── cli.h               Interface ligne de commande
│   ├── config.h            Configuration singleton
│   ├── device.h            Scanner et infos disques
│   ├── gui.h               Interface graphique (Dear ImGui)
│   ├── logger.h            Logger spdlog
│   ├── ram_patcher.h       RAM patch engine
│   ├── report.h            Génération de rapports
│   ├── sa_backup.h         Sauvegarde Service Area
│   ├── sa_parser.h         Parseur Service Area
│   ├── vendor_handler.h    Handler constructeur (abstraite)
│   └── vsc_engine.h        Moteur VSC (Vendor Specific Commands)
├── src/
│   ├── main.cpp            Point d'entrée CLI
│   ├── main_gui.cpp        Point d'entrée GUI (Win32 + DX11)
│   ├── core/               Logique métier portable
│   │   ├── ata_interface.cpp
│   │   ├── ata_utils.cpp
│   │   ├── device.cpp
│   │   ├── ram_patcher.cpp
│   │   └── vsc_engine.cpp
│   ├── os/                 Implémentations OS-spécifiques
│   │   ├── win_ata.cpp     Windows (DeviceIoControl)
│   │   └── linux_ata.cpp   Linux (ioctl SG_IO)
│   ├── sa/                 Service Area
│   │   ├── sa_backup.cpp
│   │   └── sa_parser.cpp
│   ├── ui/                 Interface utilisateur
│   │   ├── cli.cpp
│   │   ├── gui.cpp         Rendu ImGui (docking layout)
│   │   ├── logger.cpp
│   │   └── report.cpp
│   └── vendors/            Handlers constructeurs
│       ├── wd_handler.cpp
│       ├── seagate_handler.cpp
│       ├── toshiba_handler.cpp
│       └── hgst_handler.cpp
├── tests/                  Tests unitaires (Catch2)
├── resources/              Assets (icône, logo, police, images installeur)
├── scripts/                Scripts utilitaires (génération logos)
├── installer.iss           Script installeur Inno Setup
└── CMakeLists.txt
```

## Architecture

```
ATAInterface (abstraite)
├── WinATAInterface   → DeviceIoControl / ATA_PASS_THROUGH_EX
└── LinuxATAInterface → ioctl SG_IO / SAT PASS-THROUGH(16)

VendorHandler (abstraite)
├── WDHandler       → VSC 0xE0 / 0xE1
├── SeagateHandler  → SMART vendor 0xD6
├── ToshibaHandler  → 0xC0 / 0xC1
└── HGSTHandler     → 0xC0 / 0xC1

GUI (Dear ImGui + DX11)
├── Docking layout 9 panneaux
├── Thème clair / sombre
├── Police Montserrat
└── Logo intégré (texture DX11)
```

## Vendeurs supportés

| Constructeur | Méthode | Commandes VSC |
|---|---|---|
| Western Digital | Vendor Specific Commands | 0xE0 / 0xE1 |
| Seagate | SMART Vendor Page | 0xD6 |
| Toshiba | Vendor Commands | 0xC0 / 0xC1 |
| HGST | Vendor Commands | 0xC0 / 0xC1 |

## Installeur Windows

Un script [Inno Setup](installer.iss) est fourni pour générer un installeur professionnel :

```bash
# Nécessite Inno Setup 6+
iscc installer.iss
```

L'installeur inclut le CLI, le GUI, la documentation, et propose l'ajout au PATH.

## Raccourcis clavier

| Touche | Action |
|---|---|
| F5 | Rafraîchir la liste des disques |

## Régénérer les logos

```bash
python scripts/generate_logos.py
```

Génère `app.ico`, `logo_256.png`, `wizard_large.bmp` et `wizard_small.bmp` dans `resources/`.

## Licence

Usage privé uniquement — outil de diagnostic.
