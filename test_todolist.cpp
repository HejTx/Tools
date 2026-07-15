#include "todolist.hpp"
#include "sifrovani.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

void test_konec() {
    Prikaz p = rozeberPrikaz("q");
    assert(p.typ == TypPrikazu::Konec);
}

void test_vypsat_je_neznamy() {
    Prikaz p = rozeberPrikaz("v");
    assert(p.typ == TypPrikazu::Neznamy);
}

void test_ulozit() {
    Prikaz p = rozeberPrikaz("s");
    assert(p.typ == TypPrikazu::Ulozit);
}

void test_pridat_s_popisem() {
    Prikaz p = rozeberPrikaz("p Udelat si obed");
    assert(p.typ == TypPrikazu::Pridat);
    assert(p.popis == "Udelat si obed");
}

void test_pridat_bez_popisu() {
    Prikaz p = rozeberPrikaz("p");
    assert(p.typ == TypPrikazu::Pridat);
    assert(p.popis == "");
}

void test_oznacit_platne_id() {
    Prikaz p = rozeberPrikaz("o 2");
    assert(p.typ == TypPrikazu::Oznacit);
    assert(p.id == 2);
}

void test_odebrat_platne_id() {
    Prikaz p = rozeberPrikaz("r 3");
    assert(p.typ == TypPrikazu::Odebrat);
    assert(p.id == 3);
}

void test_oznacit_chybne_id() {
    Prikaz p = rozeberPrikaz("o abc");
    assert(p.typ == TypPrikazu::Neznamy);
}

void test_oznacit_chybejici_id() {
    Prikaz p = rozeberPrikaz("o");
    assert(p.typ == TypPrikazu::Neznamy);
}

void test_neznamy_prikaz() {
    Prikaz p = rozeberPrikaz("xyz");
    assert(p.typ == TypPrikazu::Neznamy);
}

void test_vykresli_prazdny_seznam() {
    std::ostringstream out;
    vykresliObrazovku(out, {}, "");
    assert(out.str() ==
        "=== Ukoly ===\n"
        "Zadne ukoly.\n"
        "\n"
        "Prikazy: p <popis> | o <id> | r <id> | s (ulozit) | q (ulozit a konec)\n"
        "> ");
}

void test_vykresli_ukoly_hotovy_sede() {
    std::ostringstream out;
    std::vector<Task> ukoly = {{1, "nakoupit", true}, {2, "uklidit", false}};
    vykresliObrazovku(out, ukoly, "");
    assert(out.str() ==
        "=== Ukoly ===\n"
        "\033[90mID: 1, Popis: nakoupit, Dokonceno: Ano\033[0m\n"
        "ID: 2, Popis: uklidit, Dokonceno: Ne\n"
        "\n"
        "Prikazy: p <popis> | o <id> | r <id> | s (ulozit) | q (ulozit a konec)\n"
        "> ");
}

void test_vykresli_se_zpravou() {
    std::ostringstream out;
    vykresliObrazovku(out, {}, "Ukol pridan.");
    assert(out.str() ==
        "=== Ukoly ===\n"
        "Zadne ukoly.\n"
        "\n"
        "Ukol pridan.\n"
        "\n"
        "Prikazy: p <popis> | o <id> | r <id> | s (ulozit) | q (ulozit a konec)\n"
        "> ");
}

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

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Nepodarilo se inicializovat libsodium.\n";
        return 1;
    }

    test_konec();
    test_vypsat_je_neznamy();
    test_ulozit();
    test_pridat_s_popisem();
    test_pridat_bez_popisu();
    test_oznacit_platne_id();
    test_odebrat_platne_id();
    test_oznacit_chybne_id();
    test_oznacit_chybejici_id();
    test_neznamy_prikaz();
    test_vykresli_prazdny_seznam();
    test_vykresli_ukoly_hotovy_sede();
    test_vykresli_se_zpravou();
    test_serializace_format();
    test_parsovani_roundtrip();
    test_parsovani_preskoci_prazdne_radky();
    test_sifrovani_roundtrip();
    test_sifrovani_prazdny_plaintext();
    test_spatne_heslo();
    test_poskozeny_soubor();
    test_zkraceny_vstup();
    test_jeSifrovany_stary_format();

    std::cout << "Vsechny testy prosly.\n";
    return 0;
}
