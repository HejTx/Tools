#pragma once

#include <sodium.h>

#include <array>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Formát šifrovaného souboru:
//   8 B  magická hlavička "TODOENC1"
//  16 B  sůl pro Argon2id (crypto_pwhash_SALTBYTES)
//  24 B  nonce (crypto_secretbox_NONCEBYTES)
//  zbytek: ciphertext = XSalsa20-Poly1305 nad textovou serializací úkolů
//          (posledních 16 B je autentizační kód, crypto_secretbox_MACBYTES)
inline const std::string SIFROVANI_HLAVICKA = "TODOENC1";

struct VysledekDesifrovani {
    std::string plaintext;
    std::vector<unsigned char> klic;
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul;
};

inline bool jeSifrovany(const std::string& obsah) {
    return obsah.size() >= SIFROVANI_HLAVICKA.size() &&
           obsah.compare(0, SIFROVANI_HLAVICKA.size(), SIFROVANI_HLAVICKA) == 0;
}

// Odvodí 32B klíč z hesla přes Argon2id. Selhání (nedostatek paměti)
// je výjimečný stav — program nemá jak pokračovat.
inline std::vector<unsigned char> odvodKlic(const std::string& heslo, const unsigned char* sul) {
    std::vector<unsigned char> klic(crypto_secretbox_KEYBYTES);
    if (crypto_pwhash(klic.data(), klic.size(),
                      heslo.c_str(), heslo.size(), sul,
                      crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE,
                      crypto_pwhash_ALG_ARGON2ID13) != 0) {
        throw std::runtime_error("Odvozeni klice selhalo (nedostatek pameti).");
    }
    return klic;
}

// Vrátí kompletní obsah šifrovaného souboru. Nonce je pokaždé nová a náhodná.
inline std::string zasifruj(const std::string& plaintext,
                            const std::vector<unsigned char>& klic,
                            const std::array<unsigned char, crypto_pwhash_SALTBYTES>& sul) {
    std::array<unsigned char, crypto_secretbox_NONCEBYTES> nonce;
    randombytes_buf(nonce.data(), nonce.size());

    std::vector<unsigned char> ciphertext(crypto_secretbox_MACBYTES + plaintext.size());
    crypto_secretbox_easy(ciphertext.data(),
                          reinterpret_cast<const unsigned char*>(plaintext.data()),
                          plaintext.size(), nonce.data(), klic.data());

    std::string vystup = SIFROVANI_HLAVICKA;
    vystup.append(reinterpret_cast<const char*>(sul.data()), sul.size());
    vystup.append(reinterpret_cast<const char*>(nonce.data()), nonce.size());
    vystup.append(reinterpret_cast<const char*>(ciphertext.data()), ciphertext.size());
    return vystup;
}

// nullopt = špatné heslo, poškozený nebo zkrácený soubor.
inline std::optional<VysledekDesifrovani> desifruj(const std::string& obsahSouboru,
                                                   const std::string& heslo) {
    const size_t hlavickaB = SIFROVANI_HLAVICKA.size();
    const size_t sulB = crypto_pwhash_SALTBYTES;
    const size_t nonceB = crypto_secretbox_NONCEBYTES;
    const size_t macB = crypto_secretbox_MACBYTES;

    if (!jeSifrovany(obsahSouboru)) return std::nullopt;
    if (obsahSouboru.size() < hlavickaB + sulB + nonceB + macB) return std::nullopt;

    const unsigned char* data = reinterpret_cast<const unsigned char*>(obsahSouboru.data());
    VysledekDesifrovani vysledek;
    std::memcpy(vysledek.sul.data(), data + hlavickaB, sulB);
    const unsigned char* nonce = data + hlavickaB + sulB;
    const unsigned char* ciphertext = data + hlavickaB + sulB + nonceB;
    const size_t ciphertextB = obsahSouboru.size() - hlavickaB - sulB - nonceB;

    vysledek.klic = odvodKlic(heslo, vysledek.sul.data());

    // +1, aby plain.data() nebyl nullptr, když je plaintext prázdný
    std::vector<unsigned char> plain(ciphertextB - macB + 1);
    if (crypto_secretbox_open_easy(plain.data(), ciphertext, ciphertextB,
                                   nonce, vysledek.klic.data()) != 0) {
        return std::nullopt;
    }

    vysledek.plaintext.assign(reinterpret_cast<const char*>(plain.data()), ciphertextB - macB);
    return vysledek;
}
