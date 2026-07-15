# Šifrování souboru ukoly.txt Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Data v `ukoly.txt` se ukládají zašifrovaně (libsodium: Argon2id + XSalsa20-Poly1305); k rozšifrování je potřeba heslo zadané skrytě při startu programu, včetně automatické migrace starého nešifrovaného souboru.

**Architecture:** Nový header-only modul `sifrovani.hpp` s čistými funkcemi nad stringy (kontejner `TODOENC1` + sůl + nonce + ciphertext) — plně testovatelný bez terminálu. V `todolist.hpp` se serializace oddělí od souborového I/O (`serializujUkoly`/`parsujUkoly`) a `ulozUkoly` zapisuje šifrovaný kontejner. Interaktivní část (skryté čtení hesla přes termios, volba nového hesla, migrace) žije jen v `todolist.cpp`.

**Tech Stack:** C++17, `g++`, libsodium (`-lsodium`), `<cassert>` testy bez frameworku, termios pro skrytý vstup.

**Spec:** `docs/superpowers/specs/2026-07-15-sifrovani-souboru-design.md`

---

### Task 1: Instalace libsodium-dev a smoke test

Prostředí: WSL2 Ubuntu. Runtime `libsodium.so.23` už v systému je, chybí hlavičky. Tento task nemění repozitář — žádný commit.

- [ ] **Step 1: Nainstaluj vývojový balíček**

Run: `sudo apt install -y libsodium-dev`
Expected: doběhne bez chyby.

- [ ] **Step 2: Ověř hlavičky a linkování smoke testem**

```bash
cat > /tmp/sodium_smoke.cpp <<'EOF'
#include <sodium.h>
#include <cstdio>
int main() {
    if (sodium_init() < 0) { std::puts("sodium_init FAILED"); return 1; }
    std::puts("sodium OK");
    return 0;
}
EOF
g++ -std=c++17 /tmp/sodium_smoke.cpp -o /tmp/sodium_smoke -lsodium && /tmp/sodium_smoke
```

Expected: vypíše `sodium OK`.

---

### Task 2: TDD extrakce serializace (serializujUkoly / parsujUkoly)

Čistý refaktor + nové čisté funkce. Chování programu se nemění; `ulozUkoly`/`nactiUkoly` zatím zůstávají nešifrované a se stejnými podpisy.

**Files:**
- Modify: `todolist.hpp` (nové funkce `serializujUkoly`, `parsujUkoly`; `ulozUkoly`/`nactiUkoly` je použijí)
- Test: `test_todolist.cpp`

- [ ] **Step 1: Přidej testy serializace do `test_todolist.cpp`**

Za `test_vykresli_se_zpravou` vlož:

```cpp
void test_serializace_format() {
    std::vector<Task> ukoly = {{1, "nakoupit", true}, {2, "uklidit", false}};
    assert(serializujUkoly(ukoly) == "1;nakoupit;1\n2;uklidit;0\n");
    assert(serializujUkoly({}) == "");
}

void test_parsovani_roundtrip() {
    std::vector<Task> ukoly = {{1, "nakoupit", true}, {2, "uklidit", false}};
    std::vector<Task> zpet = parsujUkoly(serializujUkoly(ukoly));
    assert(zpet.size() == 2);
    assert(zpet[0].id == 1 && zpet[0].description == "nakoupit" && zpet[0].done == true);
    assert(zpet[1].id == 2 && zpet[1].description == "uklidit" && zpet[1].done == false);
}

void test_parsovani_preskoci_prazdne_radky() {
    std::vector<Task> zpet = parsujUkoly("1;a;0\n\n2;b;1\n");
    assert(zpet.size() == 2);
    assert(zpet[1].description == "b" && zpet[1].done == true);
}
```

a v `main()` testu za `test_vykresli_se_zpravou();` přidej:

```cpp
    test_serializace_format();
    test_parsovani_roundtrip();
    test_parsovani_preskoci_prazdne_radky();
```

- [ ] **Step 2: Ověř, že testy selžou (chyba kompilace)**

Run: `g++ test_todolist.cpp -o test_todolist.exe`
Expected: FAIL — `error: 'serializujUkoly' was not declared in this scope`.

- [ ] **Step 3: Přidej čisté funkce do `todolist.hpp` a přepiš `ulozUkoly`/`nactiUkoly` na jejich použití**

Nahraď stávající `ulozUkoly` a `nactiUkoly` (todolist.hpp, funkce hned za `struct Task`) tímto:

```cpp
inline std::string serializujUkoly(const std::vector<Task>& ukoly) {
    std::ostringstream out;
    for (const auto& ukol : ukoly) {
        out << ukol.id << ";" << ukol.description << ";" << ukol.done << "\n";
    }
    return out.str();
}

inline std::vector<Task> parsujUkoly(const std::string& obsah) {
    std::vector<Task> ukoly;

    std::istringstream in(obsah);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string idStr, description, doneStr;

        std::getline(ss, idStr, ';');
        std::getline(ss, description, ';');
        std::getline(ss, doneStr, ';');

        Task ukol;
        ukol.id = std::stoi(idStr);
        ukol.description = description;
        ukol.done = (doneStr == "1");

        ukoly.push_back(ukol);
    }

    return ukoly;
}

// Uloží úkoly do souboru
inline void ulozUkoly(const std::vector<Task>& ukoly, const std::string& soubor) {
    std::ofstream out(soubor);
    if (!out) {
        std::cerr << "Nepodarilo se otevrit soubor pro zapis: " << soubor << "\n";
        return;
    }

    out << serializujUkoly(ukoly);
}

// Načte úkoly ze souboru
inline std::vector<Task> nactiUkoly(const std::string& soubor) {
    std::ifstream in(soubor);
    if (!in) {
        std::cerr << "Soubor " << soubor << " neexistuje, zacinam s prazdnym seznamem.\n";
        return {};
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    return parsujUkoly(ss.str());
}
```

(Původní komentáře „do souboru ukoly.md" byly zastaralé — soubor je `ukoly.txt`, komentář se opravuje na neutrální.)

- [ ] **Step 4: Ověř, že testy projdou a program se kompiluje**

Run: `g++ test_todolist.cpp -o test_todolist.exe && ./test_todolist.exe && g++ todolist.cpp -o todolist.exe`
Expected: PASS — `Vsechny testy prosly.`, žádné chyby kompilace.

- [ ] **Step 5: Commit**

```bash
git add todolist.hpp test_todolist.cpp
git commit -m "refactor: extract pure serializujUkoly/parsujUkoly from file I/O"
```

---

### Task 3: TDD modul sifrovani.hpp

Nový header-only modul s kryptografií. Od tohoto tasku se testy linkují s `-lsodium`. POZOR: testy poběží několik sekund — Argon2id (odvození klíče) trvá zhruba 0,5–1 s na volání a testy ho volají vícekrát. To je záměr (ochrana proti hádání hesel), ne zaseknutí.

**Files:**
- Create: `sifrovani.hpp`
- Test: `test_todolist.cpp`

- [ ] **Step 1: Přidej testy šifrování do `test_todolist.cpp`**

Na začátek souboru za `#include "todolist.hpp"` přidej:

```cpp
#include "sifrovani.hpp"
```

Za `test_parsovani_preskoci_prazdne_radky` vlož:

```cpp
void test_sifrovani_roundtrip() {
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul;
    randombytes_buf(sul.data(), sul.size());
    std::vector<unsigned char> klic = odvodKlic("tajneheslo", sul.data());

    std::string kontejner = zasifruj("1;nakoupit;1\n", klic, sul);
    assert(jeSifrovany(kontejner));

    std::optional<VysledekDesifrovani> vysledek = desifruj(kontejner, "tajneheslo");
    assert(vysledek.has_value());
    assert(vysledek->plaintext == "1;nakoupit;1\n");
    assert(vysledek->klic == klic);
    assert(vysledek->sul == sul);
}

void test_sifrovani_prazdny_plaintext() {
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul;
    randombytes_buf(sul.data(), sul.size());
    std::vector<unsigned char> klic = odvodKlic("tajneheslo", sul.data());

    std::string kontejner = zasifruj("", klic, sul);
    std::optional<VysledekDesifrovani> vysledek = desifruj(kontejner, "tajneheslo");
    assert(vysledek.has_value());
    assert(vysledek->plaintext == "");
}

void test_spatne_heslo() {
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul;
    randombytes_buf(sul.data(), sul.size());
    std::vector<unsigned char> klic = odvodKlic("spravne", sul.data());

    std::string kontejner = zasifruj("data", klic, sul);
    assert(!desifruj(kontejner, "spatne").has_value());
}

void test_poskozeny_soubor() {
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul;
    randombytes_buf(sul.data(), sul.size());
    std::vector<unsigned char> klic = odvodKlic("tajneheslo", sul.data());

    std::string kontejner = zasifruj("data", klic, sul);
    kontejner.back() ^= 0x01;
    assert(!desifruj(kontejner, "tajneheslo").has_value());
}

void test_zkraceny_vstup() {
    assert(!desifruj("", "x").has_value());
    assert(!desifruj("TODOENC1", "x").has_value());
    assert(!desifruj(std::string("TODOENC1") + "kratke", "x").has_value());
}

void test_jeSifrovany_stary_format() {
    assert(!jeSifrovany("1;nakoupit;0\n"));
    assert(!jeSifrovany(""));
}
```

V `main()` testu:
- Na úplný začátek těla `main()` přidej:

```cpp
    if (sodium_init() < 0) {
        std::cerr << "Nepodarilo se inicializovat libsodium.\n";
        return 1;
    }
```

- Za `test_parsovani_preskoci_prazdne_radky();` přidej:

```cpp
    test_sifrovani_roundtrip();
    test_sifrovani_prazdny_plaintext();
    test_spatne_heslo();
    test_poskozeny_soubor();
    test_zkraceny_vstup();
    test_jeSifrovany_stary_format();
```

- [ ] **Step 2: Ověř, že testy selžou (chyba kompilace)**

Run: `g++ test_todolist.cpp -o test_todolist.exe -lsodium`
Expected: FAIL — `fatal error: sifrovani.hpp: No such file or directory`.

- [ ] **Step 3: Vytvoř `sifrovani.hpp`**

```cpp
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
```

- [ ] **Step 4: Ověř, že testy projdou**

Run: `g++ test_todolist.cpp -o test_todolist.exe -lsodium && ./test_todolist.exe`
Expected: PASS — `Vsechny testy prosly.` (běh několik sekund kvůli Argon2id — viz úvod tasku).

- [ ] **Step 5: Commit**

```bash
git add sifrovani.hpp test_todolist.cpp
git commit -m "feat: add sifrovani.hpp encryption module (Argon2id + secretbox)"
```

---

### Task 4: Integrace — šifrované ukládání a heslo při startu

Přepojení všeho dohromady: `ulozUkoly` zapisuje kontejner, `nactiUkoly` se nahrazuje čtením syrového obsahu, `main()` řeší heslo, migraci a EOF. Od teď se `todolist.cpp` kompiluje s `-lsodium`.

**Files:**
- Modify: `todolist.hpp` (include `sifrovani.hpp`; `ulozUkoly` nový podpis; `nactiUkoly` → `nactiObsahSouboru`)
- Modify: `todolist.cpp` (celý soubor: heslové pomocné funkce + nový `main()`)

- [ ] **Step 1: Uprav `todolist.hpp`**

Za blok `#include` na začátku souboru přidej:

```cpp
#include "sifrovani.hpp"
```

Nahraď funkce `ulozUkoly` a `nactiUkoly` (z Tasku 2) tímto:

```cpp
// Uloží úkoly zašifrovaně (stejný klíč a sůl, nová nonce při každém zápisu)
inline void ulozUkoly(const std::vector<Task>& ukoly, const std::string& soubor,
                      const std::vector<unsigned char>& klic,
                      const std::array<unsigned char, crypto_pwhash_SALTBYTES>& sul) {
    std::ofstream out(soubor, std::ios::binary);
    if (!out) {
        std::cerr << "Nepodarilo se otevrit soubor pro zapis: " << soubor << "\n";
        return;
    }

    out << zasifruj(serializujUkoly(ukoly), klic, sul);
}

// Přečte celý soubor jako syrové bajty. nullopt = soubor nejde otevřít.
inline std::optional<std::string> nactiObsahSouboru(const std::string& soubor) {
    std::ifstream in(soubor, std::ios::binary);
    if (!in) return std::nullopt;

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
```

- [ ] **Step 2: Nahraď celý obsah `todolist.cpp` tímto**

```cpp
#include "todolist.hpp"

#include <termios.h>
#include <unistd.h>

// Přečte řádek se skrytým echem (termios). Vrací nullopt při EOF.
// Když stdin není terminál (pipe v testech), čte normálně.
std::optional<std::string> prectiHeslo(const std::string& vyzva) {
    std::cout << vyzva << std::flush;

    termios puvodni;
    bool jeTerminal = (tcgetattr(STDIN_FILENO, &puvodni) == 0);
    if (jeTerminal) {
        termios bezEcha = puvodni;
        bezEcha.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &bezEcha);
    }

    std::string heslo;
    bool nacteno = static_cast<bool>(std::getline(std::cin, heslo));

    if (jeTerminal) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &puvodni);
        std::cout << "\n";
    }

    if (!nacteno) return std::nullopt;
    return heslo;
}

// Dvakrát se zeptá na nové heslo; opakuje, dokud se zadání neshodují
// a nejsou prázdná. nullopt = EOF.
std::optional<std::string> zvolNoveHeslo() {
    while (true) {
        std::optional<std::string> prvni = prectiHeslo("Zvol nove heslo: ");
        if (!prvni) return std::nullopt;
        std::optional<std::string> druhe = prectiHeslo("Heslo znovu: ");
        if (!druhe) return std::nullopt;

        if (*prvni != *druhe) {
            std::cout << "Hesla se neshoduji, zkus to znovu.\n";
        } else if (prvni->empty()) {
            std::cout << "Heslo nesmi byt prazdne.\n";
        } else {
            return prvni;
        }
    }
}

// Zvolí nové heslo, vygeneruje čerstvou sůl a odvodí klíč. false = EOF.
bool nastavNovyKlic(std::vector<unsigned char>& klic,
                    std::array<unsigned char, crypto_pwhash_SALTBYTES>& sul) {
    std::optional<std::string> heslo = zvolNoveHeslo();
    if (!heslo) return false;
    randombytes_buf(sul.data(), sul.size());
    klic = odvodKlic(*heslo, sul.data());
    sodium_memzero(heslo->data(), heslo->size());
    return true;
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Nepodarilo se inicializovat libsodium.\n";
        return 1;
    }

    const std::string soubor = "ukoly.txt";

    std::vector<Task> ukoly;
    std::vector<unsigned char> klic;
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul{};

    std::optional<std::string> obsah = nactiObsahSouboru(soubor);
    if (!obsah) {
        std::cout << "Soubor " << soubor << " neexistuje, zaklada se novy sifrovany seznam.\n";
        if (!nastavNovyKlic(klic, sul)) return 0;
    } else if (jeSifrovany(*obsah)) {
        while (true) {
            std::optional<std::string> heslo = prectiHeslo("Heslo: ");
            if (!heslo) return 0;
            std::optional<VysledekDesifrovani> vysledek = desifruj(*obsah, *heslo);
            sodium_memzero(heslo->data(), heslo->size());
            if (vysledek) {
                ukoly = parsujUkoly(vysledek->plaintext);
                klic = std::move(vysledek->klic);
                sul = vysledek->sul;
                break;
            }
            std::cout << "Spatne heslo nebo poskozeny soubor, zkus to znovu.\n";
        }
    } else {
        std::cout << "Soubor " << soubor << " je v nesifrovanem formatu a bude zasifrovan.\n";
        ukoly = parsujUkoly(*obsah);
        if (!nastavNovyKlic(klic, sul)) return 0;
    }

    std::string radek;
    std::string zprava;
    while (true) {
        std::cout << "\033[2J\033[H";
        vykresliObrazovku(std::cout, ukoly, zprava);
        if (!std::getline(std::cin, radek)) break;

        Prikaz prikaz = rozeberPrikaz(radek);
        if (prikaz.typ == TypPrikazu::Konec) break;

        switch (prikaz.typ) {
            case TypPrikazu::Pridat:
                if (prikaz.popis.find(';') != std::string::npos) {
                    zprava = "Popis nesmi obsahovat znak ';'.";
                } else {
                    pridatUkol(ukoly, prikaz.popis);
                    zprava = "Ukol pridan.";
                }
                break;
            case TypPrikazu::Oznacit:
                if (oznacitUkolDokonceny(ukoly, prikaz.id)) {
                    zprava = "Ukol " + std::to_string(prikaz.id) + " oznacen jako hotovy.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            case TypPrikazu::Odebrat:
                if (odebratUkol(ukoly, prikaz.id)) {
                    zprava = "Ukol " + std::to_string(prikaz.id) + " odebran.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            case TypPrikazu::Ulozit:
                ulozUkoly(ukoly, soubor, klic, sul);
                zprava = "Ukoly ulozeny.";
                break;
            case TypPrikazu::Konec:
                break;
            default:
                zprava = "Neznamy prikaz.";
                break;
        }
    }

    ulozUkoly(ukoly, soubor, klic, sul);
    std::cout << "\nUkoly ulozeny. Nashledanou.\n";
    return 0;
}
```

- [ ] **Step 3: Zkompiluj obojí**

Run: `g++ test_todolist.cpp -o test_todolist.exe -lsodium && ./test_todolist.exe && g++ todolist.cpp -o todolist.exe -lsodium`
Expected: PASS — `Vsechny testy prosly.`, žádné chyby kompilace.

- [ ] **Step 4: Zálohuj reálná data**

Run: `[ -f ukoly.txt ] && mv ukoly.txt ukoly.txt.bak || true`

- [ ] **Step 5: Ruční test — nový soubor**

Run: `printf 'tajne\ntajne\np nakoupit\nq\n' | ./todolist.exe | cat -v`
Expected: výstup obsahuje `zaklada se novy sifrovany seznam`, `Zvol nove heslo:`, `Ukol pridan.` a `Ukoly ulozeny. Nashledanou.` (odvození klíče chvíli trvá — Argon2id).

Run: `head -c 8 ukoly.txt; echo; grep -a -c nakoupit ukoly.txt || echo "plaintext nenalezen"`
Expected: první řádek `TODOENC1`, potom `0` nebo `plaintext nenalezen` (popis úkolu v souboru NENÍ čitelný).

- [ ] **Step 6: Ruční test — špatné a správné heslo**

Run: `printf 'spatne\ntajne\nq\n' | ./todolist.exe | cat -v`
Expected: výstup obsahuje `Spatne heslo nebo poskozeny soubor, zkus to znovu.`, po správném hesle seznam s `nakoupit` a rozlučku.

- [ ] **Step 7: Ruční test — neshodná hesla při zakládání**

Run: `rm ukoly.txt && printf 'a\nb\ntajne2\ntajne2\nq\n' | ./todolist.exe | cat -v`
Expected: výstup obsahuje `Hesla se neshoduji, zkus to znovu.` a potom normální běh.

- [ ] **Step 8: Ruční test — migrace nešifrovaného souboru**

Run: `printf '1;stary ukol;0\n' > ukoly.txt && printf 'tajne3\ntajne3\nq\n' | ./todolist.exe | cat -v`
Expected: výstup obsahuje `je v nesifrovanem formatu a bude zasifrovan` a v seznamu `stary ukol`.

Run: `printf 'tajne3\nq\n' | ./todolist.exe | cat -v && head -c 8 ukoly.txt; echo`
Expected: po zadání hesla seznam obsahuje `stary ukol`; soubor začíná `TODOENC1`.

- [ ] **Step 9: Ruční test — EOF při zadávání hesla**

Run: `./todolist.exe < /dev/null; echo "exit=$?"`
Expected: program se zeptá `Heslo: `, na EOF skončí s `exit=0` (exit kód je programu — žádná pipe za ním), soubor zůstane nezměněný (stále `TODOENC1` + data z kroku 8).

- [ ] **Step 10: Obnov reálná data**

Run: `rm -f ukoly.txt && [ -f ukoly.txt.bak ] && mv ukoly.txt.bak ukoly.txt || true`

(Pozn.: obnovený `ukoly.txt` je stále starý nešifrovaný formát — migraci si uživatel projde sám při prvním ostrém spuštění, kde si zvolí své skutečné heslo.)

- [ ] **Step 11: Commit**

```bash
git add todolist.hpp todolist.cpp
git commit -m "feat: encrypt task file with password (libsodium, Argon2id + secretbox)"
```
