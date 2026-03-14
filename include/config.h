#pragma once
/*
 * config.h — Singleton de configuration globale (F11)
 */

#include <string>

class Config {
public:
    static Config& get() {
        static Config instance;
        return instance;
    }

    bool dry_run   = false;    /* --dry-run : simule les ecritures                */
    bool verbose   = false;    /* --verbose : affiche les commandes ATA en hexa   */
    bool debug     = false;    /* --debug   : log niveau DEBUG                    */
    bool quiet     = false;    /* --quiet   : minimum de sortie console           */
    bool show_pwd  = false;    /* --show-password : affiche les mots de passe SA  */
    bool force     = false;    /* --force   : desactive le backup obligatoire     */
    std::string uart_port;     /* --uart COM3 ou /dev/ttyUSB0 pour Seagate F3     */
    std::string device_path;   /* chemin du disque specifie en argument            */

    /* Liste des opcodes consideres comme ECRITURE (bloquees en dry-run) */
    static bool is_write_command(uint8_t opcode) {
        switch (opcode) {
            case 0xF1: /* SECURITY SET PASSWORD   */
            case 0xF2: /* SECURITY UNLOCK         */
            case 0xF3: /* SECURITY ERASE PREPARE  */
            case 0xF4: /* SECURITY ERASE UNIT     */
            case 0xF5: /* SECURITY FREEZE LOCK    */
            case 0xF6: /* SECURITY DISABLE        */
            case 0x46: /* WD WRITE RAM            */
            case 0xE0: /* WD VENDOR ENTER         */
            case 0xE1: /* WD VENDOR EXIT          */
            case 0xC0: /* TOSHIBA VENDOR ENTER    */
            case 0xC1: /* TOSHIBA VENDOR EXIT     */
                return true;
            default:
                return false;
        }
    }

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
};
