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

void test_slozene_id() {
    Prikaz p = rozeberPrikaz("o 2.3");
    assert(p.typ == TypPrikazu::Oznacit && p.seznamUkolu == 2 && p.id == 3);
    Prikaz proste = rozeberPrikaz("o 3");
    assert(proste.typ == TypPrikazu::Oznacit && proste.seznamUkolu == -1 && proste.id == 3);
    assert(rozeberPrikaz("o 2.").typ == TypPrikazu::Neznamy);
    assert(rozeberPrikaz("o .3").typ == TypPrikazu::Neznamy);
    assert(rozeberPrikaz("o 2.x").typ == TypPrikazu::Neznamy);
    Prikaz presun = rozeberPrikaz("m 2.3 4");
    assert(presun.typ == TypPrikazu::PresunoutUkol);
    assert(presun.seznamUkolu == 2 && presun.id == 3 && presun.id2 == 4);
}

void test_termin_prikaz() {
    Prikaz p = rozeberPrikaz("t 2 18/07/26");
    assert(p.typ == TypPrikazu::Termin && p.id == 2 && p.popis == "18/07/26");
    Prikaz slozeny = rozeberPrikaz("t 2.3 18/07/26");
    assert(slozeny.typ == TypPrikazu::Termin && slozeny.seznamUkolu == 2 && slozeny.id == 3);
    Prikaz mazani = rozeberPrikaz("t 2");
    assert(mazani.typ == TypPrikazu::Termin && mazani.popis == "");
    assert(rozeberPrikaz("t").typ == TypPrikazu::Neznamy);
}

void test_razeni_prikaz() {
    assert(rozeberPrikaz("z").typ == TypPrikazu::PrepnoutRazeni);
}

void test_priorita_prikaz() {
    Prikaz p = rozeberPrikaz("pr 3 1");
    assert(p.typ == TypPrikazu::Priorita && p.id == 3 && p.popis == "1");
    Prikaz slozeny = rozeberPrikaz("pr 2.3 1");
    assert(slozeny.typ == TypPrikazu::Priorita && slozeny.seznamUkolu == 2 && slozeny.id == 3);
    Prikaz reset = rozeberPrikaz("pr 3");
    assert(reset.typ == TypPrikazu::Priorita && reset.popis == "");
    assert(rozeberPrikaz("pr").typ == TypPrikazu::Neznamy);
}

void test_priorita_tiebreaker() {
    std::vector<Task> ukoly = {{1, "nizka", false, "18/07/26", 3},
                               {2, "vysoka", false, "18/07/26", 1},
                               {3, "normalni", false, "18/07/26"}};
    std::vector<Task> serazene = serazeneUkoly(ukoly, 2);
    assert(serazene[0].id == 2 && serazene[1].id == 3 && serazene[2].id == 1);
    // rezim 1 prioritu ignoruje
    assert(serazeneUkoly(ukoly, 1)[0].id == 1);

    StavSeznamu stav;
    stav.seznamy = {{1, "A", {{1, "nizka", false, "18/07/26", 3}}},
                    {2, "B", {{1, "vysoka", false, "18/07/26", 1}}}};
    stav.razeni = 2;
    assert(sestavPrehled(stav)[0].seznamId == 2);  // vysoka prvni pri stejnem terminu
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
        "Seznamy: [0] Vse (50.0%) | [1] Nakup (50.0%) | \033[1m>[2] Ukoly EQ tyden (0.0%)<\033[0m\n");
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
        "  e <id> <popis>   Upravi popis ukolu.\n"
        "  t <id> <datum>   Nastavi termin (dd/mm/yy); t <id> bez data termin smaze.\n"
        "  m <id> <sid>     Presune ukol do seznamu <sid>.\n"
        "  c                Odstrani hotove ukoly (v prehledu 0 ze vsech seznamu).\n"
        "\n"
        "PRIKAZY SEZNAMU\n"
        "  n <nazev>        Zalozi novy seznam a prepne na nej.\n"
        "  v <id>           Prepne na seznam podle ID; v 0 = prehled vsech ukolu.\n"
        "  j <id> <nazev>   Prejmenuje seznam.\n"
        "  d                Smaze aktivni seznam (bez potvrzeni!).\n"
        "  d <id>           Smaze seznam podle ID.\n"
        "\n"
        "OSTATNI\n"
        "  u                Vrati posledni zmenu (u znovu = zpet).\n"
        "  z                Prepina razeni: podle ID / podle terminu.\n"
        "  s                Ulozi vsechny seznamy.\n"
        "  q                Ulozi a ukonci program.\n"
        "  zh               Zmeni heslo souboru.\n"
        "  h                Zobrazi tuto napovedu.\n"
        "\n"
        "Ukoly lze adresovat i jako <seznam>.<ukol> (napr. o 2.3) - v prehledu 0 je to nutne.\n"
        "\n"
        "Pokracuj stiskem Enteru...\n");
}

void test_vytiskni_seznamy_zalamovani() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Alfa", {}}, {2, "Beta", {}}, {3, "Gama", {}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vytiskniSeznamy(out, stav, 45);
    // 9 + 14 (Vse) + 3 + 17 (aktivni) = 43 <= 45; Beta by presahla -> zalom
    assert(out.str() ==
        "Seznamy: [0] Vse (0.0%) | \033[1m>[1] Alfa (0.0%)<\033[0m\n"
        "         [2] Beta (0.0%) | [3] Gama (0.0%)\n");
}

void test_vytiskni_seznamy_uzka_obrazovka() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Alfa", {}}, {2, "Beta", {}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vytiskniSeznamy(out, stav, 10);
    // kazda polozka sirsi nez sirka -> jedna na radek
    assert(out.str() ==
        "Seznamy: [0] Vse (0.0%)\n"
        "         \033[1m>[1] Alfa (0.0%)<\033[0m\n"
        "         [2] Beta (0.0%)\n");
}

void test_vytiskni_prosly_cervene() {
    // dnes = 19/07/26 -> klic 260719
    std::ostringstream prosly;
    vytiskniUkol(prosly, {1, "prosly", false, "18/07/26"}, "", 260719);
    assert(prosly.str() ==
        "\033[31mID: 1, Popis: prosly, Dokonceno: Ne, Termin: 18/07/26\033[0m\n");

    std::ostringstream dnesni;
    vytiskniUkol(dnesni, {2, "dnesni", false, "19/07/26"}, "", 260719);
    assert(dnesni.str() == "ID: 2, Popis: dnesni, Dokonceno: Ne, Termin: 19/07/26\n");

    std::ostringstream hotovy;  // hotovy prosly zustava sedy
    vytiskniUkol(hotovy, {3, "hotovy", true, "18/07/26"}, "", 260719);
    assert(hotovy.str() ==
        "\033[90mID: 3, Popis: hotovy, Dokonceno: Ano, Termin: 18/07/26\033[0m\n");

    std::ostringstream vypnuto;  // dnes = 0 -> kontrola vypnuta
    vytiskniUkol(vypnuto, {4, "prosly", false, "18/07/26"});
    assert(vypnuto.str() == "ID: 4, Popis: prosly, Dokonceno: Ne, Termin: 18/07/26\n");
}

void test_vykresli_s_proslym() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Ukoly", {{1, "prosly", false, "18/07/26"}}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vykresliObrazovku(out, stav, "", 80, 260719);
    assert(out.str().find(
        "\033[31mID: 1, Popis: prosly, Dokonceno: Ne, Termin: 18/07/26\033[0m\n")
        != std::string::npos);
}

void test_vykresli_prehled() {
    StavSeznamu stav;
    stav.seznamy = {
        {1, "A", {{1, "a1", false, "20/08/26"}}},
        {2, "B", {{1, "b1", true}}},
    };
    stav.aktivniId = 0;
    stav.razeni = 1;
    std::ostringstream out;
    vykresliObrazovku(out, stav, "");
    std::string obrazovka = out.str();
    assert(obrazovka.find("=== Vse === (razeni: ID)\n") != std::string::npos);
    assert(obrazovka.find("ID: 1.1, Popis: a1, Dokonceno: Ne, Termin: 20/08/26\n") != std::string::npos);
    assert(obrazovka.find("\033[90mID: 2.1, Popis: b1, Dokonceno: Ano\033[0m\n") != std::string::npos);
    assert(obrazovka.find("Seznamy: \033[1m>[0] Vse (50.0%)<\033[0m") != std::string::npos);
}

void test_vykresli_prazdny_seznam() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Ukoly", {}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vykresliObrazovku(out, stav, "");
    assert(out.str() ==
        "Seznamy: [0] Vse (0.0%) | \033[1m>[1] Ukoly (0.0%)<\033[0m\n"
        "=== Ukoly === (razeni: ID)\n"
        "Zadne ukoly.\n"
        "\n"
        "\033[90mukol: p pridat · o hotovo · r odebrat · e upravit · m presunout · t termin\n"
        "seznam: n novy · v vybrat · j prejmenovat · d smazat · c uklidit\n"
        "jine: u zpet · z razeni · s ulozit · zh heslo · q konec · h napoveda\033[0m\n"
        "> ");
}

void test_vykresli_ukoly_hotovy_sede() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Ukoly", {{1, "nakoupit", true}, {2, "uklidit", false}}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vykresliObrazovku(out, stav, "");
    assert(out.str() ==
        "Seznamy: [0] Vse (50.0%) | \033[1m>[1] Ukoly (50.0%)<\033[0m\n"
        "=== Ukoly === (razeni: ID)\n"
        "\033[90mID: 1, Popis: nakoupit, Dokonceno: Ano\033[0m\n"
        "ID: 2, Popis: uklidit, Dokonceno: Ne\n"
        "\n"
        "\033[90mukol: p pridat · o hotovo · r odebrat · e upravit · m presunout · t termin\n"
        "seznam: n novy · v vybrat · j prejmenovat · d smazat · c uklidit\n"
        "jine: u zpet · z razeni · s ulozit · zh heslo · q konec · h napoveda\033[0m\n"
        "> ");
}

void test_vykresli_se_zpravou() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Ukoly", {}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vykresliObrazovku(out, stav, "Ukol pridan.");
    assert(out.str() ==
        "Seznamy: [0] Vse (0.0%) | \033[1m>[1] Ukoly (0.0%)<\033[0m\n"
        "=== Ukoly === (razeni: ID)\n"
        "Zadne ukoly.\n"
        "\n"
        "Ukol pridan.\n"
        "\n"
        "\033[90mukol: p pridat · o hotovo · r odebrat · e upravit · m presunout · t termin\n"
        "seznam: n novy · v vybrat · j prejmenovat · d smazat · c uklidit\n"
        "jine: u zpet · z razeni · s ulozit · zh heslo · q konec · h napoveda\033[0m\n"
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
    assert(serializujUkoly(ukoly) == "1;nakoupit;1;;2\n2;uklidit;0;;2\n");
    assert(serializujUkoly({}) == "");
}

void test_platny_termin() {
    assert(jePlatnyTermin("18/07/26"));
    assert(jePlatnyTermin("01/01/00"));
    assert(!jePlatnyTermin(""));
    assert(!jePlatnyTermin("18/7/26"));
    assert(!jePlatnyTermin("18-07-26"));
    assert(!jePlatnyTermin("32/07/26"));
    assert(!jePlatnyTermin("18/13/26"));
    assert(!jePlatnyTermin("00/07/26"));
    assert(!jePlatnyTermin("aa/bb/cc"));
}

void test_klic_terminu() {
    assert(klicTerminu("18/07/26") == 260718);
    assert(klicTerminu("19/07/26") > klicTerminu("18/07/26"));
    assert(klicTerminu("01/01/27") > klicTerminu("31/12/26"));
    assert(klicTerminu("") > klicTerminu("31/12/99"));  // bez terminu nakonec
    // poškozený termín (ručně upravený soubor) nesmí shodit řazení
    assert(klicTerminu("garbage!") == klicTerminu(""));
    std::vector<Task> poskozene = {{1, "a", false, "xx/yy/zz"}, {2, "b", false, "18/07/26"}};
    assert(serazeneUkoly(poskozene, 2)[0].id == 2);
}

void test_serializace_terminu() {
    std::vector<Task> ukoly = {{1, "a", false, "18/07/26"}, {2, "b", true}};
    assert(serializujUkoly(ukoly) == "1;a;0;18/07/26;2\n2;b;1;;2\n");
    std::vector<Task> zpet = parsujUkoly(serializujUkoly(ukoly));
    assert(zpet[0].termin == "18/07/26" && zpet[1].termin == "");
    // stary 3-polovy format -> bez terminu
    std::vector<Task> stare = parsujUkoly("1;nakoupit;1\n");
    assert(stare[0].termin == "" && stare[0].done);
}

void test_priorita_serializace_a_nastaveni() {
    std::vector<Task> ukoly = {{1, "a", false, "18/07/26", 1}, {2, "b", false}};
    assert(serializujUkoly(ukoly) == "1;a;0;18/07/26;1\n2;b;0;;2\n");
    std::vector<Task> zpet = parsujUkoly(serializujUkoly(ukoly));
    assert(zpet[0].priorita == 1 && zpet[1].priorita == 2);
    // stare formaty a nesmysly -> 2
    assert(parsujUkoly("1;a;0;18/07/26\n")[0].priorita == 2);
    assert(parsujUkoly("1;a;0\n")[0].priorita == 2);
    assert(parsujUkoly("1;a;0;;9\n")[0].priorita == 2);
    assert(parsujUkoly("1;a;0;;x\n")[0].priorita == 2);

    assert(nastavPrioritu(ukoly, 2, 3));
    assert(ukoly[1].priorita == 3);
    assert(!nastavPrioritu(ukoly, 99, 1));
}

void test_vytiskni_ukol_s_terminem() {
    std::ostringstream out;
    vytiskniUkol(out, {1, "mleko", false, "18/07/26"});
    assert(out.str() == "ID: 1, Popis: mleko, Dokonceno: Ne, Termin: 18/07/26\n");
    std::ostringstream out2;
    vytiskniUkol(out2, {2, "chleba", false}, "3.");
    assert(out2.str() == "ID: 3.2, Popis: chleba, Dokonceno: Ne\n");
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
        "@razeni;1\n"
        "#seznam;1;Nakup\n"
        "1;mleko;0;;2\n"
        "2;chleba;1;;2\n"
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

void test_uloz_seznam_do_souboru() {
    Seznam seznam;
    seznam.id = 1;
    seznam.nazev = "Test";
    seznam.ukoly = {{1, "nakoupit", false, "18/07/26"}};
    randombytes_buf(seznam.sul.data(), seznam.sul.size());
    seznam.klic = odvodKlic("tajneheslo", seznam.sul.data());

    const std::string cesta = "test_trezor_docasny.txt";
    assert(ulozSeznamDoSouboru(seznam, cesta));
    std::optional<std::string> obsah = nactiObsahSouboru(cesta);
    assert(obsah && jeSifrovany(*obsah));
    std::optional<VysledekDesifrovani> vysledek = desifruj(*obsah, "tajneheslo");
    assert(vysledek && vysledek->plaintext == serializujUkoly(seznam.ukoly));
    std::remove(cesta.c_str());

    // selhani: neexistujici adresar (hlaska na stderr je ocekavana)
    assert(!ulozSeznamDoSouboru(seznam, "neexistujici_adresar/x.txt"));
}

void test_kontrola_nazvu_seznamu() {
    assert(zkontrolujNazevSeznamu("Nakup") == "");
    assert(zkontrolujNazevSeznamu("Ukoly EQ tyden") == "");
    assert(zkontrolujNazevSeznamu("") == "Nazev nesmi byt prazdny.");
    assert(zkontrolujNazevSeznamu("a;b") == "Nazev nesmi obsahovat znak ';'.");
    assert(zkontrolujNazevSeznamu("a/b") == "Nazev nesmi obsahovat znak '/'.");
    assert(zkontrolujNazevSeznamu(".skryty") == "Nazev nesmi zacinat teckou.");
}

void test_nastaveni_roundtrip() {
    Nastaveni n;
    n.razeni = 2;
    n.posledni = "Ukoly EQ tyden";
    assert(serializujNastaveni(n) == "razeni;2\nposledni;Ukoly EQ tyden\n");
    Nastaveni zpet = parsujNastaveni(serializujNastaveni(n));
    assert(zpet.razeni == 2 && zpet.posledni == "Ukoly EQ tyden");
    Nastaveni prazdne = parsujNastaveni("");
    assert(prazdne.razeni == 1 && prazdne.posledni == "");
    assert(parsujNastaveni("razeni;9\n").razeni == 1);
}

void test_razeni_roundtrip() {
    StavSeznamu stav;
    stav.seznamy = {{1, "A", {}}};
    stav.aktivniId = 1;
    stav.razeni = 2;
    assert(serializujSeznamy(stav) == "@aktivni;1\n@razeni;2\n#seznam;1;A\n");
    assert(parsujSeznamy(serializujSeznamy(stav)).razeni == 2);
    // chybejici/neplatna hodnota -> 1
    assert(parsujSeznamy("@aktivni;1\n#seznam;1;A\n").razeni == 1);
    assert(parsujSeznamy("@aktivni;1\n@razeni;7\n#seznam;1;A\n").razeni == 1);
}

void test_serazene_ukoly() {
    std::vector<Task> ukoly = {{1, "bez", false},
                               {2, "pozdejsi", false, "20/08/26"},
                               {3, "drivejsi", false, "18/07/26"}};
    std::vector<Task> podleId = serazeneUkoly(ukoly, 1);
    assert(podleId[0].id == 1 && podleId[2].id == 3);
    std::vector<Task> podleTerminu = serazeneUkoly(ukoly, 2);
    assert(podleTerminu[0].id == 3);  // nejblizsi termin prvni
    assert(podleTerminu[1].id == 2);
    assert(podleTerminu[2].id == 1);  // bez terminu nakonec

    // hotove nakonec, i kdyz maji nejblizsi termin
    std::vector<Task> sHotovym = {{1, "hotovy blizky", true, "01/01/26"},
                                  {2, "nehotovy pozdejsi", false, "20/08/26"},
                                  {3, "hotovy pozdni", true, "31/12/26"}};
    std::vector<Task> serazene = serazeneUkoly(sHotovym, 2);
    assert(serazene[0].id == 2);                        // nehotovy prvni
    assert(serazene[1].id == 1 && serazene[2].id == 3); // hotove podle terminu
}

void test_sestav_prehled() {
    StavSeznamu stav;
    stav.seznamy = {
        {1, "A", {{1, "a1", false}, {2, "a2", false, "20/08/26"}}},
        {2, "B", {{1, "b1", false, "18/07/26"}}},
    };
    stav.razeni = 1;
    std::vector<PolozkaPrehledu> podleId = sestavPrehled(stav);
    assert(podleId.size() == 3);
    assert(podleId[0].seznamId == 1 && podleId[0].ukol.id == 1);
    assert(podleId[2].seznamId == 2);
    stav.razeni = 2;
    std::vector<PolozkaPrehledu> podleTerminu = sestavPrehled(stav);
    assert(podleTerminu[0].seznamId == 2);              // 18/07 prvni
    assert(podleTerminu[1].ukol.termin == "20/08/26");
    assert(podleTerminu[2].ukol.termin == "");          // bez terminu nakonec

    stav.seznamy[0].ukoly.push_back({3, "hotovy blizky", true, "01/01/26"});
    std::vector<PolozkaPrehledu> sHotovym = sestavPrehled(stav);
    assert(sHotovym.back().ukol.id == 3);               // hotovy az nakonec
}

void test_serializace_zamceneho() {
    StavSeznamu stav;
    stav.aktivniId = 1;
    stav.seznamy = {{1, "A", {}}};
    Seznam pahyl;
    pahyl.id = 2;
    pahyl.nazev = "Tajny";
    pahyl.odemceno = false;
    stav.seznamy.push_back(pahyl);
    assert(serializujSeznamy(stav) ==
        "@aktivni;1\n@razeni;1\n#seznam;1;A\n#seznam;2;Tajny;zamceno\n");
}

void test_smazat_posledni_odemceny() {
    StavSeznamu stav;
    stav.seznamy = {{1, "A", {}}};
    Seznam pahyl;
    pahyl.id = 2;
    pahyl.nazev = "Tajny";
    pahyl.odemceno = false;
    stav.seznamy.push_back(pahyl);
    stav.aktivniId = 1;
    assert(smazatSeznam(stav, 1));
    assert(stav.aktivniId == 0);          // zadny odemceny -> prehled
    assert(stav.seznamy.size() == 1);     // zadna auto-nahrada "Ukoly"
}

void test_prehled_preskoci_zamcene() {
    StavSeznamu stav;
    stav.seznamy = {{1, "A", {{1, "a1", false}}}};
    Seznam pahyl;
    pahyl.id = 2;
    pahyl.nazev = "Tajny";
    pahyl.odemceno = false;
    stav.seznamy.push_back(pahyl);
    assert(sestavPrehled(stav).size() == 1);
}

void test_vytiskni_seznamy_zamceny() {
    StavSeznamu stav;
    stav.seznamy = {{1, "A", {{1, "a1", true}}}};
    Seznam pahyl;
    pahyl.id = 2;
    pahyl.nazev = "Tajny";
    pahyl.odemceno = false;
    stav.seznamy.push_back(pahyl);
    stav.aktivniId = 1;
    std::ostringstream out;
    vytiskniSeznamy(out, stav);
    assert(out.str() ==
        "Seznamy: [0] Vse (100.0%) | \033[1m>[1] A (100.0%)<\033[0m | [2] Tajny (zamceno)\n");
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
    assert(stav.seznamy.empty());   // zadna auto-nahrada
    assert(stav.aktivniId == 0);    // aktivni je prehled
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

void test_presun_ukolu() {
    StavSeznamu stav;
    stav.seznamy = {
        {1, "A", {{1, "prvni", true}, {2, "druhy", false}}},
        {2, "B", {{1, "cizi", false}}},
    };
    stav.aktivniId = 1;
    assert(presunUkol(stav, 1, 1, 2) == 0);
    assert(stav.seznamy[0].ukoly.size() == 1);
    assert(stav.seznamy[1].ukoly.size() == 2);
    assert(stav.seznamy[1].ukoly[1].id == 2);          // nove ID v cili
    assert(stav.seznamy[1].ukoly[1].description == "prvni");
    assert(stav.seznamy[1].ukoly[1].done == true);     // done se zachova
    assert(presunUkol(stav, 1, 99, 2) == 1);           // ukol nenalezen
    assert(presunUkol(stav, 1, 99, 1) == 1);           // neexistence ukolu ma prednost
    assert(presunUkol(stav, 1, 2, 99) == 2);           // seznam nenalezen
    assert(presunUkol(stav, 1, 2, 1) == 3);            // cil == zdroj
}

void test_nastav_termin() {
    std::vector<Task> ukoly = {{1, "a", false}};
    assert(nastavTermin(ukoly, 1, "18/07/26"));
    assert(ukoly[0].termin == "18/07/26");
    assert(nastavTermin(ukoly, 1, ""));
    assert(ukoly[0].termin == "");
    assert(!nastavTermin(ukoly, 99, "18/07/26"));
}

void test_presun_z_jineho_seznamu() {
    StavSeznamu stav;
    stav.seznamy = {{1, "A", {{1, "a1", false}}}, {2, "B", {{1, "b1", false}}}};
    stav.aktivniId = 1;
    assert(presunUkol(stav, 2, 1, 1) == 0);  // z B do A, aktivni nehraje roli
    assert(stav.seznamy[0].ukoly.size() == 2 && stav.seznamy[1].ukoly.empty());
    assert(stav.seznamy[0].ukoly[1].description == "b1");
    assert(presunUkol(stav, 99, 1, 1) == 1); // neexistujici zdroj = ukol nenalezen
}

void test_seznam_nula_stav() {
    StavSeznamu stav;
    stav.seznamy = {{1, "A", {}}, {2, "B", {}}};
    stav.aktivniId = 1;
    assert(vybratSeznam(stav, 0) && stav.aktivniId == 0);
    assert(smazatSeznam(stav, 1) && stav.aktivniId == 0);   // prehled zustava
    // @aktivni;0 prezije round-trip
    StavSeznamu zpet = parsujSeznamy(serializujSeznamy(stav));
    assert(zpet.aktivniId == 0);
    assert(smazatSeznam(stav, 2) && stav.aktivniId == 0);   // i po poslednim
    assert(stav.seznamy.empty());                           // zadna auto-nahrada
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
    test_slozene_id();
    test_termin_prikaz();
    test_razeni_prikaz();
    test_priorita_prikaz();
    test_priorita_tiebreaker();
    test_napoveda_prikaz();
    test_neznamy_prikaz();
    test_vytiskni_napovedu();
    test_vytiskni_seznamy();
    test_vytiskni_seznamy_zalamovani();
    test_vytiskni_seznamy_uzka_obrazovka();
    test_vytiskni_prosly_cervene();
    test_vykresli_s_proslym();
    test_vykresli_prehled();
    test_vykresli_prazdny_seznam();
    test_vykresli_ukoly_hotovy_sede();
    test_vykresli_se_zpravou();
    test_procenta();
    test_serializace_format();
    test_platny_termin();
    test_klic_terminu();
    test_serializace_terminu();
    test_vytiskni_ukol_s_terminem();
    test_priorita_serializace_a_nastaveni();
    test_parsovani_roundtrip();
    test_parsovani_preskoci_prazdne_radky();
    test_parsovani_preskoci_poskozeny_radek();
    test_serializace_seznamu();
    test_parsovani_seznamu_roundtrip();
    test_migrace_stareho_formatu();
    test_parsovani_prazdneho_obsahu();
    test_neplatne_aktivni_id();
    test_kontrola_nazvu_seznamu();
    test_nastaveni_roundtrip();
    test_serializace_zamceneho();
    test_smazat_posledni_odemceny();
    test_prehled_preskoci_zamcene();
    test_vytiskni_seznamy_zamceny();
    test_razeni_roundtrip();
    test_serazene_ukoly();
    test_sestav_prehled();
    test_uloz_seznam_do_souboru();
    test_pridat_seznam();
    test_vybrat_seznam();
    test_prejmenovat_seznam();
    test_smazat_neaktivni_seznam();
    test_smazat_aktivni_seznam();
    test_smazat_posledni_seznam();
    test_upravit_ukol();
    test_vycisti_hotove();
    test_presun_ukolu();
    test_nastav_termin();
    test_presun_z_jineho_seznamu();
    test_seznam_nula_stav();
    test_sifrovani_roundtrip();
    test_sifrovani_prazdny_plaintext();
    test_spatne_heslo();
    test_poskozeny_soubor();
    test_zkraceny_vstup();
    test_jeSifrovany_stary_format();

    std::cout << "Vsechny testy prosly.\n";
    return 0;
}
