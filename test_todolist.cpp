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

void test_novy_seznam_prikaz() {
    Prikaz p = rozeberPrikaz("n Ukoly EQ tyden");
    assert(p.typ == TypPrikazu::NovySeznam);
    assert(p.popis == "Ukoly EQ tyden");
    Prikaz prazdny = rozeberPrikaz("n");
    assert(prazdny.typ == TypPrikazu::NovySeznam && prazdny.popis == "");
}

void test_vybrat_seznam_prikaz() {
    Prikaz p = rozeberPrikaz("v 2");
    assert(p.typ == TypPrikazu::VybratSeznam && p.id == 2);
    assert(rozeberPrikaz("v abc").typ == TypPrikazu::Neznamy);
    // "v" bez ID zůstává Neznamy (test_vypsat_je_neznamy to už kontroluje)
}

void test_prejmenovat_prikaz() {
    Prikaz p = rozeberPrikaz("j 2 Novy nazev");
    assert(p.typ == TypPrikazu::PrejmenovatSeznam);
    assert(p.id == 2 && p.popis == "Novy nazev");
    assert(rozeberPrikaz("j").typ == TypPrikazu::Neznamy);
    assert(rozeberPrikaz("j abc X").typ == TypPrikazu::Neznamy);
    Prikaz bezNazvu = rozeberPrikaz("j 2");
    assert(bezNazvu.typ == TypPrikazu::PrejmenovatSeznam && bezNazvu.popis == "");
}

void test_smazat_prikaz() {
    Prikaz bezId = rozeberPrikaz("d");
    assert(bezId.typ == TypPrikazu::SmazatSeznam && bezId.id == -1);
    Prikaz sId = rozeberPrikaz("d 3");
    assert(sId.typ == TypPrikazu::SmazatSeznam && sId.id == 3);
    assert(rozeberPrikaz("d abc").typ == TypPrikazu::Neznamy);
    // "-1" a "0" nesmí kolidovat se sentinelem pro "smazat aktivní"
    assert(rozeberPrikaz("d -1").typ == TypPrikazu::Neznamy);
    assert(rozeberPrikaz("d 0").typ == TypPrikazu::Neznamy);
}

void test_upravit_prikaz() {
    Prikaz p = rozeberPrikaz("e 2 novy text ukolu");
    assert(p.typ == TypPrikazu::UpravitUkol);
    assert(p.id == 2 && p.popis == "novy text ukolu");
    assert(rozeberPrikaz("e").typ == TypPrikazu::Neznamy);
    assert(rozeberPrikaz("e abc x").typ == TypPrikazu::Neznamy);
}

void test_presunout_prikaz() {
    Prikaz p = rozeberPrikaz("m 3 2");
    assert(p.typ == TypPrikazu::PresunoutUkol);
    assert(p.id == 3 && p.id2 == 2);
    assert(rozeberPrikaz("m").typ == TypPrikazu::Neznamy);
    assert(rozeberPrikaz("m 3").typ == TypPrikazu::Neznamy);
    assert(rozeberPrikaz("m 3 abc").typ == TypPrikazu::Neznamy);
}

void test_bezargumentove_prikazy() {
    assert(rozeberPrikaz("c").typ == TypPrikazu::VycistitHotove);
    assert(rozeberPrikaz("u").typ == TypPrikazu::Zpet);
    assert(rozeberPrikaz("zh").typ == TypPrikazu::ZmenaHesla);
}

void test_napoveda_prikaz() {
    assert(rozeberPrikaz("h").typ == TypPrikazu::Napoveda);
    assert(rozeberPrikaz("h cokoli").typ == TypPrikazu::Napoveda);  // zbytek se ignoruje
}

void test_neznamy_prikaz() {
    Prikaz p = rozeberPrikaz("xyz");
    assert(p.typ == TypPrikazu::Neznamy);
}

void test_vytiskni_seznamy() {
    StavSeznamu stav;
    stav.seznamy = {
        {1, "Nakup", {{1, "mleko", true}, {2, "chleba", false}}},
        {2, "Ukoly EQ tyden", {}},
    };
    stav.aktivniId = 2;
    std::ostringstream out;
    vytiskniSeznamy(out, stav);
    assert(out.str() ==
        "Seznamy: [1] Nakup (50.0%) | \033[1m>[2] Ukoly EQ tyden (0.0%)<\033[0m\n");
}

void test_vytiskni_napovedu() {
    std::ostringstream out;
    vytiskniNapovedu(out);
    assert(out.str() ==
        "TODOLIST(1)                        Napoveda\n"
        "\n"
        "PRIKAZY UKOLU\n"
        "  p <popis>        Prida novy ukol do aktivniho seznamu.\n"
        "  o <id>           Oznaci ukol jako hotovy.\n"
        "  r <id>           Odebere ukol ze seznamu.\n"
        "\n"
        "PRIKAZY SEZNAMU\n"
        "  n <nazev>        Zalozi novy seznam a prepne na nej.\n"
        "  v <id>           Prepne na seznam podle ID.\n"
        "  j <id> <nazev>   Prejmenuje seznam.\n"
        "  d                Smaze aktivni seznam (bez potvrzeni!).\n"
        "  d <id>           Smaze seznam podle ID.\n"
        "\n"
        "OSTATNI\n"
        "  s                Ulozi vsechny seznamy.\n"
        "  q                Ulozi a ukonci program.\n"
        "  h                Zobrazi tuto napovedu.\n"
        "\n"
        "Pokracuj stiskem Enteru...\n");
}

void test_vykresli_prazdny_seznam() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Ukoly", {}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vykresliObrazovku(out, stav, "");
    assert(out.str() ==
        "Seznamy: \033[1m>[1] Ukoly (0.0%)<\033[0m\n"
        "=== Ukoly ===\n"
        "Zadne ukoly.\n"
        "\n"
        "Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q | h\n"
        "> ");
}

void test_vykresli_ukoly_hotovy_sede() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Ukoly", {{1, "nakoupit", true}, {2, "uklidit", false}}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vykresliObrazovku(out, stav, "");
    assert(out.str() ==
        "Seznamy: \033[1m>[1] Ukoly (50.0%)<\033[0m\n"
        "=== Ukoly ===\n"
        "\033[90mID: 1, Popis: nakoupit, Dokonceno: Ano\033[0m\n"
        "ID: 2, Popis: uklidit, Dokonceno: Ne\n"
        "\n"
        "Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q | h\n"
        "> ");
}

void test_vykresli_se_zpravou() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Ukoly", {}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vykresliObrazovku(out, stav, "Ukol pridan.");
    assert(out.str() ==
        "Seznamy: \033[1m>[1] Ukoly (0.0%)<\033[0m\n"
        "=== Ukoly ===\n"
        "Zadne ukoly.\n"
        "\n"
        "Ukol pridan.\n"
        "\n"
        "Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q | h\n"
        "> ");
}

void test_procenta() {
    assert(formatujProcenta({}) == "0.0%");
    assert(formatujProcenta({{1, "a", true}, {2, "b", false}}) == "50.0%");
    assert(formatujProcenta({{1, "a", true}, {2, "b", false}, {3, "c", false}}) == "33.3%");
    assert(formatujProcenta({{1, "a", true}, {2, "b", true}, {3, "c", false}}) == "66.7%");
    assert(formatujProcenta({{1, "a", true}}) == "100.0%");
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

void test_parsovani_preskoci_poskozeny_radek() {
    std::vector<Task> zpet = parsujUkoly("abc;x;0\n2;b;1\n");
    assert(zpet.size() == 1);
    assert(zpet[0].id == 2 && zpet[0].description == "b");
}

void test_serializace_seznamu() {
    StavSeznamu stav;
    stav.aktivniId = 2;
    stav.seznamy = {
        {1, "Nakup", {{1, "mleko", false}, {2, "chleba", true}}},
        {2, "Prace", {}},
    };
    assert(serializujSeznamy(stav) ==
        "@aktivni;2\n"
        "#seznam;1;Nakup\n"
        "1;mleko;0\n"
        "2;chleba;1\n"
        "#seznam;2;Prace\n");
}

void test_parsovani_seznamu_roundtrip() {
    StavSeznamu stav;
    stav.aktivniId = 2;
    stav.seznamy = {
        {1, "Nakup", {{1, "mleko", false}, {2, "chleba", true}}},
        {2, "Ukoly EQ tyden", {}},
    };
    StavSeznamu zpet = parsujSeznamy(serializujSeznamy(stav));
    assert(zpet.aktivniId == 2);
    assert(zpet.seznamy.size() == 2);
    assert(zpet.seznamy[0].id == 1 && zpet.seznamy[0].nazev == "Nakup");
    assert(zpet.seznamy[0].ukoly.size() == 2);
    assert(zpet.seznamy[0].ukoly[1].description == "chleba" && zpet.seznamy[0].ukoly[1].done);
    assert(zpet.seznamy[1].id == 2 && zpet.seznamy[1].nazev == "Ukoly EQ tyden");
    assert(zpet.seznamy[1].ukoly.empty());
}

void test_migrace_stareho_formatu() {
    StavSeznamu stav = parsujSeznamy("1;nakoupit;1\n2;uklidit;0\n");
    assert(stav.seznamy.size() == 1);
    assert(stav.seznamy[0].id == 1 && stav.seznamy[0].nazev == "Ukoly");
    assert(stav.seznamy[0].ukoly.size() == 2);
    assert(stav.aktivniId == 1);
}

void test_parsovani_prazdneho_obsahu() {
    StavSeznamu stav = parsujSeznamy("");
    assert(stav.seznamy.size() == 1);
    assert(stav.seznamy[0].nazev == "Ukoly" && stav.seznamy[0].ukoly.empty());
    assert(stav.aktivniId == 1);
}

void test_neplatne_aktivni_id() {
    StavSeznamu stav = parsujSeznamy("@aktivni;99\n#seznam;3;Prace\n1;report;0\n");
    assert(stav.aktivniId == 3);  // neexistující ID -> první seznam
}

void test_uloz_seznamy_uspech_a_selhani() {
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul;
    randombytes_buf(sul.data(), sul.size());
    std::vector<unsigned char> klic = odvodKlic("tajneheslo", sul.data());
    StavSeznamu stav;
    stav.seznamy = {{1, "Ukoly", {{1, "nakoupit", false}}}};
    stav.aktivniId = 1;

    const std::string soubor = "test_uloz_docasny.txt";
    assert(ulozSeznamy(stav, soubor, klic, sul));
    std::optional<std::string> obsah = nactiObsahSouboru(soubor);
    assert(obsah && jeSifrovany(*obsah));
    std::optional<VysledekDesifrovani> vysledek = desifruj(*obsah, "tajneheslo");
    assert(vysledek && vysledek->plaintext == serializujSeznamy(stav));
    std::remove(soubor.c_str());

    // selhani: neexistujici adresar (hlaska na stderr je ocekavana)
    assert(!ulozSeznamy(stav, "neexistujici_adresar/ukoly.txt", klic, sul));
}

void test_pridat_seznam() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Ukoly", {}}};
    stav.aktivniId = 1;
    int id = pridatSeznam(stav, "Prace");
    assert(id == 2);
    assert(stav.seznamy.size() == 2 && stav.seznamy[1].nazev == "Prace");
    assert(stav.aktivniId == 2);  // nový seznam se rovnou otevře
}

void test_vybrat_seznam() {
    StavSeznamu stav;
    stav.seznamy = {{1, "A", {}}, {2, "B", {}}};
    stav.aktivniId = 1;
    assert(vybratSeznam(stav, 2) && stav.aktivniId == 2);
    assert(!vybratSeznam(stav, 99) && stav.aktivniId == 2);
}

void test_prejmenovat_seznam() {
    StavSeznamu stav;
    stav.seznamy = {{1, "A", {}}};
    assert(prejmenovatSeznam(stav.seznamy, 1, "Nakup"));
    assert(stav.seznamy[0].nazev == "Nakup");
    assert(!prejmenovatSeznam(stav.seznamy, 99, "X"));
}

void test_smazat_neaktivni_seznam() {
    StavSeznamu stav;
    stav.seznamy = {{1, "A", {}}, {2, "B", {}}};
    stav.aktivniId = 1;
    assert(smazatSeznam(stav, 2));
    assert(stav.seznamy.size() == 1 && stav.aktivniId == 1);
    assert(!smazatSeznam(stav, 99));
}

void test_smazat_aktivni_seznam() {
    StavSeznamu stav;
    stav.seznamy = {{1, "A", {}}, {2, "B", {}}, {3, "C", {}}};
    stav.aktivniId = 2;
    assert(smazatSeznam(stav, 2));
    assert(stav.aktivniId == 1);  // první zbývající
}

void test_smazat_posledni_seznam() {
    StavSeznamu stav;
    stav.seznamy = {{5, "Jediny", {{1, "ukol", false}}}};
    stav.aktivniId = 5;
    assert(smazatSeznam(stav, 5));
    assert(stav.seznamy.size() == 1);
    assert(stav.seznamy[0].id == 1 && stav.seznamy[0].nazev == "Ukoly");
    assert(stav.seznamy[0].ukoly.empty());
    assert(stav.aktivniId == 1);
}

void test_upravit_ukol() {
    std::vector<Task> ukoly = {{1, "stary", true}};
    assert(upravitUkol(ukoly, 1, "novy"));
    assert(ukoly[0].description == "novy" && ukoly[0].done == true && ukoly[0].id == 1);
    assert(!upravitUkol(ukoly, 99, "x"));
}

void test_vycisti_hotove() {
    std::vector<Task> ukoly = {{1, "a", true}, {2, "b", false}, {3, "c", true}};
    assert(vycistiHotove(ukoly) == 2);
    assert(ukoly.size() == 1 && ukoly[0].description == "b");
    assert(vycistiHotove(ukoly) == 0);
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
    test_novy_seznam_prikaz();
    test_vybrat_seznam_prikaz();
    test_prejmenovat_prikaz();
    test_smazat_prikaz();
    test_upravit_prikaz();
    test_presunout_prikaz();
    test_bezargumentove_prikazy();
    test_napoveda_prikaz();
    test_neznamy_prikaz();
    test_vytiskni_napovedu();
    test_vytiskni_seznamy();
    test_vykresli_prazdny_seznam();
    test_vykresli_ukoly_hotovy_sede();
    test_vykresli_se_zpravou();
    test_procenta();
    test_serializace_format();
    test_parsovani_roundtrip();
    test_parsovani_preskoci_prazdne_radky();
    test_parsovani_preskoci_poskozeny_radek();
    test_serializace_seznamu();
    test_parsovani_seznamu_roundtrip();
    test_migrace_stareho_formatu();
    test_parsovani_prazdneho_obsahu();
    test_neplatne_aktivni_id();
    test_uloz_seznamy_uspech_a_selhani();
    test_pridat_seznam();
    test_vybrat_seznam();
    test_prejmenovat_seznam();
    test_smazat_neaktivni_seznam();
    test_smazat_aktivni_seznam();
    test_smazat_posledni_seznam();
    test_upravit_ukol();
    test_vycisti_hotove();
    test_sifrovani_roundtrip();
    test_sifrovani_prazdny_plaintext();
    test_spatne_heslo();
    test_poskozeny_soubor();
    test_zkraceny_vstup();
    test_jeSifrovany_stary_format();

    std::cout << "Vsechny testy prosly.\n";
    return 0;
}
