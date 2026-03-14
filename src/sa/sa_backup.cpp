/*
 * sa_backup.cpp — Backup et restauration Service Area (F13)
 */

#include "sa_backup.h"
#include "config.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>
#include <array>

/* SHA-256 minimal (pas de dependance externe) */
namespace {

/* SHA-256 implementation simplifiee — suffisante pour un hash de fichier */
struct SHA256 {
    uint32_t state[8]{};
    uint8_t  data[64]{};
    uint32_t datalen = 0;
    uint64_t bitlen  = 0;

    static constexpr uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    static uint32_t rotr(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    static uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    static uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    static uint32_t ep0(uint32_t x) { return rotr(x,2) ^ rotr(x,13) ^ rotr(x,22); }
    static uint32_t ep1(uint32_t x) { return rotr(x,6) ^ rotr(x,11) ^ rotr(x,25); }
    static uint32_t sig0(uint32_t x) { return rotr(x,7) ^ rotr(x,18) ^ (x>>3); }
    static uint32_t sig1(uint32_t x) { return rotr(x,17) ^ rotr(x,19) ^ (x>>10); }

    void init() {
        datalen = 0; bitlen = 0;
        state[0]=0x6a09e667; state[1]=0xbb67ae85; state[2]=0x3c6ef372; state[3]=0xa54ff53a;
        state[4]=0x510e527f; state[5]=0x9b05688c; state[6]=0x1f83d9ab; state[7]=0x5be0cd19;
    }

    void transform() {
        uint32_t m[64], a, b, c, d, e, f, g, h, t1, t2;
        for (int i=0; i<16; ++i)
            m[i] = (uint32_t(data[i*4])<<24) | (uint32_t(data[i*4+1])<<16)
                  | (uint32_t(data[i*4+2])<<8) | uint32_t(data[i*4+3]);
        for (int i=16; i<64; ++i)
            m[i] = sig1(m[i-2]) + m[i-7] + sig0(m[i-15]) + m[i-16];
        a=state[0]; b=state[1]; c=state[2]; d=state[3];
        e=state[4]; f=state[5]; g=state[6]; h=state[7];
        for (int i=0; i<64; ++i) {
            t1 = h + ep1(e) + ch(e,f,g) + k[i] + m[i];
            t2 = ep0(a) + maj(a,b,c);
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
        state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
    }

    void update(const uint8_t* buf, size_t len) {
        for (size_t i=0; i<len; ++i) {
            data[datalen++] = buf[i];
            if (datalen == 64) { transform(); bitlen += 512; datalen = 0; }
        }
    }

    void final(uint8_t hash[32]) {
        uint32_t i = datalen;
        data[i++] = 0x80;
        if (datalen < 56) {
            while (i < 56) data[i++] = 0;
        } else {
            while (i < 64) data[i++] = 0;
            transform();
            memset(data, 0, 56);
        }
        bitlen += datalen * 8;
        for (int j = 0; j < 8; ++j)
            data[63 - j] = static_cast<uint8_t>(bitlen >> (j * 8));
        transform();
        for (int j = 0; j < 4; ++j) {
            hash[j]    = (state[0] >> (24-j*8)) & 0xff;
            hash[j+4]  = (state[1] >> (24-j*8)) & 0xff;
            hash[j+8]  = (state[2] >> (24-j*8)) & 0xff;
            hash[j+12] = (state[3] >> (24-j*8)) & 0xff;
            hash[j+16] = (state[4] >> (24-j*8)) & 0xff;
            hash[j+20] = (state[5] >> (24-j*8)) & 0xff;
            hash[j+24] = (state[6] >> (24-j*8)) & 0xff;
            hash[j+28] = (state[7] >> (24-j*8)) & 0xff;
        }
    }
};

} /* anonymous namespace */

/* ==========================================================================
 *  Implementation SABackup
 * ========================================================================== */

std::string SABackup::make_backup_filename(const char* serial) {
    char buf[128];
    time_t now = time(nullptr);
    struct tm t;
#ifdef _WIN32
    localtime_s(&t, &now);
#else
    localtime_r(&now, &t);
#endif
    snprintf(buf, sizeof(buf), "hdd_backup_%s_%04d%02d%02d_%02d%02d%02d.bin",
             serial,
             t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
             t.tm_hour, t.tm_min, t.tm_sec);
    return buf;
}

std::string SABackup::sha256_file(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";

    SHA256 ctx;
    ctx.init();

    uint8_t buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        ctx.update(buf, n);
    }
    fclose(f);

    uint8_t hash[32];
    ctx.final(hash);

    char hex[65];
    for (int i = 0; i < 32; ++i)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);
    hex[64] = '\0';

    return hex;
}

std::string SABackup::backup_sa(VendorHandler* handler,
                                 const char* serial, const char* model) {
    std::string filename = make_backup_filename(serial);

    FILE* f = fopen(filename.c_str(), "wb");
    if (!f) return "";

    /* Ecrire le header */
    BackupHeader hdr{};
    memcpy(hdr.magic, "HDDBACK", 8);
    hdr.version = 1;
    snprintf(hdr.vendor, sizeof(hdr.vendor), "%s", handler->vendor_name());
    snprintf(hdr.serial, sizeof(hdr.serial), "%s", serial);
    snprintf(hdr.model, sizeof(hdr.model), "%s", model);
    hdr.timestamp = static_cast<uint32_t>(time(nullptr));

    /* Liste des modules a sauvegarder */
    const uint8_t modules[] = { 0x20, 0x2A, 0x01, 0x02 };
    std::vector<BackupModule> saved;

    for (uint8_t mod_id : modules) {
        std::vector<uint8_t> buf(512, 0);
        if (handler->read_sa_module(mod_id, buf)) {
            saved.push_back({mod_id, buf});
        }
    }

    hdr.module_count = static_cast<uint32_t>(saved.size());

    fwrite(&hdr, sizeof(hdr), 1, f);

    /* Ecrire chaque module : 1 byte module_id + 512 bytes data */
    for (const auto& m : saved) {
        fwrite(&m.module_id, 1, 1, f);
        fwrite(m.data.data(), 1, m.data.size(), f);
    }

    fclose(f);
    return filename;
}

bool SABackup::restore_sa(VendorHandler* /* handler */, const std::string& /* backup_path */) {
    /* TODO: implementer la restauration SA */
    /* Necessite des commandes VSC d'ecriture specifiques a chaque constructeur */
    printf("  Restauration SA : non encore implemente.\n");
    return false;
}
