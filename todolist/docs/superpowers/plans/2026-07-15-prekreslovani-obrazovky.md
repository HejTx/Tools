# Překreslování obrazovky (mini-TUI) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Po každém příkazu smazat a překreslit obrazovku (seznam úkolů nahoře, zpráva o výsledku, nápověda a prompt dole), aby výstup příkazu neodsunula nápověda; hotové úkoly se vypisují šedě a příkaz `v` se odebírá.

**Architecture:** Nová čistá funkce `vykresliObrazovku(std::ostream&, ukoly, zprava)` v `todolist.hpp` vypisuje celou obrazovku (bez ANSI mazání) — testovatelná přes `std::ostringstream`. Mazání obrazovky (`"\033[2J\033[H"`) vypisuje jen `main()` v `todolist.cpp`, který drží proměnnou `zprava` s výsledkem poslední akce. Šedé obarvení hotových úkolů (`"\033[90m"`…`"\033[0m"`) dělá `vytiskniUkol`, který nově píše do předaného streamu.

**Tech Stack:** C++17, `g++`, `<cassert>` pro testy (žádný externí test framework), ANSI escape sekvence (WSL/Linux/Windows Terminal).

**Spec:** `docs/superpowers/specs/2026-07-15-prekreslovani-obrazovky-design.md`

---

### Task 1: Dokončit a commitnout rozpracovaný příkaz `s` (Ulozit)

V pracovním stromu je necommitnutá implementace příkazu `s` (enum hodnota `TypPrikazu::Ulozit`, větev v `rozeberPrikaz`, case v `main()`), ale chybí jí test parseru. Doplnit test a commitnout, ať další tasky staví na čistém stromu.

**Files:**
- Modify: `test_todolist.cpp` (přidat test + volání v `main`)
- Commit také: `todolist.cpp`, `todolist.hpp` (už změněné v pracovním stromu)

- [ ] **Step 1: Přidej do `test_todolist.cpp` test příkazu `s`**

Za funkci `test_vypsat` vlož:

```cpp
void test_ulozit() {
    Prikaz p = rozeberPrikaz("s");
    assert(p.typ == TypPrikazu::Ulozit);
}
```

a v `main()` testu za `test_vypsat();` přidej řádek:

```cpp
    test_ulozit();
```

- [ ] **Step 2: Ověř, že testy projdou**

Run: `g++ test_todolist.cpp -o test_todolist.exe && ./test_todolist.exe`
Expected: PASS — `Vsechny testy prosly.` (implementace `s` už v `todolist.hpp` existuje).

- [ ] **Step 3: Ověř, že se hlavní program kompiluje**

Run: `g++ todolist.cpp -o todolist.exe`
Expected: žádné chyby.

- [ ] **Step 4: Commit**

```bash
git add todolist.hpp todolist.cpp test_todolist.cpp
git commit -m "feat: add 's' command to save tasks without quitting"
```

---

### Task 2: TDD render funkce `vykresliObrazovku` + šedé hotové úkoly

**Files:**
- Modify: `todolist.hpp` (`vytiskniUkol`, `vytiskniUkoly` dostanou `std::ostream&`; nová `vykresliObrazovku`)
- Modify: `todolist.cpp:28` (přizpůsobit volání `vytiskniUkoly` novému podpisu)
- Test: `test_todolist.cpp`

- [ ] **Step 1: Přidej do `test_todolist.cpp` testy pro `vykresliObrazovku`, která ještě neexistuje**

Na začátek souboru přidej include:

```cpp
#include <sstream>
```

Za `test_neznamy_prikaz` vlož:

```cpp
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
```

a v `main()` testu za `test_neznamy_prikaz();` přidej:

```cpp
    test_vykresli_prazdny_seznam();
    test_vykresli_ukoly_hotovy_sede();
    test_vykresli_se_zpravou();
```

- [ ] **Step 2: Ověř, že testy selžou (chyba kompilace)**

Run: `g++ test_todolist.cpp -o test_todolist.exe`
Expected: FAIL — `error: 'vykresliObrazovku' was not declared in this scope`.

- [ ] **Step 3: Uprav `vytiskniUkol`/`vytiskniUkoly` v `todolist.hpp` a přidej `vykresliObrazovku`**

Nahraď stávající `vytiskniUkol` a `vytiskniUkoly` (řádky 61–69) tímto:

```cpp
inline void vytiskniUkol(std::ostream& out, const Task& ukol) {
    if (ukol.done) out << "\033[90m";
    out << "ID: " << ukol.id << ", Popis: " << ukol.description
        << ", Dokonceno: " << (ukol.done ? "Ano" : "Ne");
    if (ukol.done) out << "\033[0m";
    out << "\n";
}

inline void vytiskniUkoly(std::ostream& out, const std::vector<Task>& ukoly) {
    for (const auto& ukol : ukoly) {
        vytiskniUkol(out, ukol);
    }
}
```

Na konec `todolist.hpp` (za `rozeberPrikaz`) přidej:

```cpp
inline void vykresliObrazovku(std::ostream& out,
                              const std::vector<Task>& ukoly,
                              const std::string& zprava) {
    out << "=== Ukoly ===\n";
    if (ukoly.empty()) {
        out << "Zadne ukoly.\n";
    } else {
        vytiskniUkoly(out, ukoly);
    }
    out << "\n";
    if (!zprava.empty()) {
        out << zprava << "\n\n";
    }
    out << "Prikazy: p <popis> | o <id> | r <id> | s (ulozit) | q (ulozit a konec)\n"
        << "> ";
}
```

- [ ] **Step 4: Přizpůsob volání v `todolist.cpp`**

V case `TypPrikazu::Vypsat` (řádek 28) změň `vytiskniUkoly(ukoly);` na:

```cpp
                vytiskniUkoly(std::cout, ukoly);
```

- [ ] **Step 5: Ověř, že testy projdou a program se kompiluje**

Run: `g++ test_todolist.cpp -o test_todolist.exe && ./test_todolist.exe && g++ todolist.cpp -o todolist.exe`
Expected: PASS — `Vsechny testy prosly.`, žádné chyby kompilace.

- [ ] **Step 6: Commit**

```bash
git add todolist.hpp todolist.cpp test_todolist.cpp
git commit -m "feat: add vykresliObrazovku screen renderer with gray done tasks"
```

---

### Task 3: Odebrat příkaz `v`

**Files:**
- Modify: `todolist.hpp` (enum, `rozeberPrikaz`)
- Modify: `todolist.cpp` (case `Vypsat`, řádek nápovědy s `v`)
- Test: `test_todolist.cpp`

- [ ] **Step 1: Uprav test — `"v"` je nově neznámý příkaz**

V `test_todolist.cpp` nahraď celou funkci `test_vypsat`:

```cpp
void test_vypsat_je_neznamy() {
    Prikaz p = rozeberPrikaz("v");
    assert(p.typ == TypPrikazu::Neznamy);
}
```

a v `main()` testu změň volání `test_vypsat();` na `test_vypsat_je_neznamy();`.

- [ ] **Step 2: Ověř, že test selže**

Run: `g++ test_todolist.cpp -o test_todolist.exe && ./test_todolist.exe`
Expected: FAIL — assert v `test_vypsat_je_neznamy` spadne (parser stále vrací `Vypsat`).

- [ ] **Step 3: Odeber `Vypsat` z `todolist.hpp` a `todolist.cpp`**

V `todolist.hpp`:
- V enumu nahraď řádek `enum class TypPrikazu { Pridat, Vypsat, Oznacit, Odebrat, Konec, Neznamy, Ulozit };` za:

```cpp
enum class TypPrikazu { Pridat, Oznacit, Odebrat, Konec, Neznamy, Ulozit };
```

- V `rozeberPrikaz` smaž větev:

```cpp
    } else if (token == "v") {
        prikaz.typ = TypPrikazu::Vypsat;
```

V `todolist.cpp`:
- Smaž celý case:

```cpp
            case TypPrikazu::Vypsat:
                vytiskniUkoly(std::cout, ukoly);
                break;
```

- Ve `vypisNapovedu` smaž řádek `<< "  v          - vypsat ukoly\n"` (celá funkce zmizí v Tasku 4, ale strom musí zůstat konzistentní).

- [ ] **Step 4: Ověř, že testy projdou a program se kompiluje**

Run: `g++ test_todolist.cpp -o test_todolist.exe && ./test_todolist.exe && g++ todolist.cpp -o todolist.exe`
Expected: PASS — `Vsechny testy prosly.`, žádné chyby kompilace.

- [ ] **Step 5: Commit**

```bash
git add todolist.hpp todolist.cpp test_todolist.cpp
git commit -m "feat: remove 'v' command, list is always visible after redraw"
```

---

### Task 4: Překreslovací smyčka v main()

**Files:**
- Modify: `todolist.cpp` (celý soubor)

- [ ] **Step 1: Nahraď celý obsah `todolist.cpp` tímto**

```cpp
#include "todolist.hpp"

int main() {
    const std::string soubor = "ukoly.txt";
    std::vector<Task> ukoly = nactiUkoly(soubor);

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
                ulozUkoly(ukoly, soubor);
                zprava = "Ukoly ulozeny.";
                break;
            case TypPrikazu::Konec:
                break;
            default:
                zprava = "Neznamy prikaz.";
                break;
        }
    }

    ulozUkoly(ukoly, soubor);
    std::cout << "\nUkoly ulozeny. Nashledanou.\n";
    return 0;
}
```

(Funkce `vypisNapovedu` tímto zaniká — nahrazuje ji `vykresliObrazovku`.)

- [ ] **Step 2: Zkompiluj**

Run: `g++ todolist.cpp -o todolist.exe`
Expected: žádné chyby ani varování.

- [ ] **Step 3: Zálohuj reálná data před ručními testy**

Run: `[ -f ukoly.txt ] && mv ukoly.txt ukoly.txt.bak || true`

- [ ] **Step 4: Ruční test golden path (ANSI kódy zviditelní `cat -v`)**

Run: `printf 'p nakoupit\np uklidit\no 1\nq\n' | ./todolist.exe | cat -v`

Expected: výstup obsahuje `^[[2J^[[H` před každým překreslením, `Ukol pridan.`, `Ukol 1 oznacen jako hotovy.`, šedý řádek `^[[90mID: 1, Popis: nakoupit, Dokonceno: Ano^[[0m` a na konci `Ukoly ulozeny. Nashledanou.`.

- [ ] **Step 5: Ruční test chybových stavů**

Run: `printf 'o 99\nr 99\nblah\np spatny;popis\nq\n' | ./todolist.exe | cat -v`

Expected: výstup postupně obsahuje `Ukol s ID 99 nenalezen.` (dvakrát), `Neznamy prikaz.`, `Popis nesmi obsahovat znak ';'.` — program nespadne.

- [ ] **Step 6: Ověř perzistenci a obnov data**

Run: `cat ukoly.txt`
Expected: obsahuje `1;nakoupit;1` a `2;uklidit;0` (z kroku 4; krok 5 nic nepřidal).

Run: `[ -f ukoly.txt.bak ] && mv ukoly.txt.bak ukoly.txt || true`

- [ ] **Step 7: Spusť ještě jednou testy**

Run: `./test_todolist.exe`
Expected: PASS — `Vsechny testy prosly.`

- [ ] **Step 8: Commit**

```bash
git add todolist.cpp
git commit -m "feat: redraw screen after each command (mini-TUI loop)"
```
