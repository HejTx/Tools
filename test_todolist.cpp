#include "todolist.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

void test_konec() {
    Prikaz p = rozeberPrikaz("q");
    assert(p.typ == TypPrikazu::Konec);
}

void test_vypsat() {
    Prikaz p = rozeberPrikaz("v");
    assert(p.typ == TypPrikazu::Vypsat);
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

int main() {
    test_konec();
    test_vypsat();
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

    std::cout << "Vsechny testy prosly.\n";
    return 0;
}
