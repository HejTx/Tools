# Interaktivní smyčka v main() Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Nahradit statické `main()` v `todolist.cpp` interaktivní smyčkou, která čte příkazy z `std::cin`, dokud uživatel nezadá `q`, a vykonává je nad existujícím seznamem úkolů.

**Architecture:** Rozdělíme kód do `todolist.hpp` (header-only, `inline` funkce — struct `Task`, I/O funkce nad úkoly a nová čistá parsovací funkce `rozeberPrikaz`) a `todolist.cpp` (jen `main()` s interaktivní smyčkou). Parsovací logika je čistá funkce bez vstupu/výstupu, takže je testovatelná assert-based testy v samostatném `test_todolist.cpp`, aniž by testy musely simulovat `std::cin`.

**Tech Stack:** C++17, `g++`, `<cassert>` pro testy (žádný externí test framework).

---

### Task 1: Extrahovat existující kód do todolist.hpp (čistý refaktor, beze změny chování)

**Files:**
- Create: `todolist.hpp`
- Modify: `todolist.cpp:1-104` (nahradit obsah jedním `#include`)

- [ ] **Step 1: Vytvoř `todolist.hpp` s existující logikou přesunutou z `todolist.cpp`**

```cpp
#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

struct Task {
    int id;
    std::string description;
    bool done = false;
};

// Uloží úkoly do souboru ukoly.md
inline void ulozUkoly(const std::vector<Task>& ukoly, const std::string& soubor) {
    std::ofstream out(soubor);
    if (!out) {
        std::cerr << "Nepodarilo se otevrit soubor pro zapis: " << soubor << "\n";
        return;
    }

    for (const auto& ukol : ukoly) {
        out << ukol.id << ";" << ukol.description << ";" << ukol.done << "\n";
    }
}

// Načte úkoly ze souboru ukoly.md
inline std::vector<Task> nactiUkoly(const std::string& soubor) {
    std::vector<Task> ukoly;

    std::ifstream in(soubor);
    if (!in) {
        std::cerr << "Soubor " << soubor << " neexistuje, zacinam s prazdnym seznamem.\n";
        return ukoly;
    }

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

inline void vytiskniUkol(const Task& ukol) {
    std::cout << "ID: " << ukol.id << ", Popis: " << ukol.description << ", Dokonceno: " << (ukol.done ? "Ano" : "Ne") << "\n";
}

inline void vytiskniUkoly(const std::vector<Task>& ukoly) {
    for (const auto& ukol : ukoly) {
        vytiskniUkol(ukol);
    }
}

inline bool pridatUkol(std::vector<Task>& ukoly, const std::string& description) {
    int newId = 1;
    if (!ukoly.empty()) {
        newId = ukoly.back().id + 1;
    }

    Task novyUkol;
    novyUkol.id = newId;
    novyUkol.description = description;
    novyUkol.done = false;

    ukoly.push_back(novyUkol);
    return true;
}

inline bool odebratUkol(std::vector<Task>& ukoly, int id) {
    auto it = std::remove_if(ukoly.begin(), ukoly.end(), [id](const Task& ukol) {
        return ukol.id == id;
    });

    if (it != ukoly.end()) {
        ukoly.erase(it, ukoly.end());
        return true;
    }
    return false;
}

inline bool oznacitUkolDokonceny(std::vector<Task>& ukoly, int id) {
    for (auto& ukol : ukoly) {
        if (ukol.id == id) {
            ukol.done = true;
            return true;
        }
    }
    return false;
}
```

- [ ] **Step 2: Nahraď obsah `todolist.cpp` includem headeru a zachovej stávající `main()`**

```cpp
#include "todolist.hpp"

int main() {
    const std::string soubor = "ukoly.txt";

    std::vector<Task> ukoly = nactiUkoly(soubor);

    ulozUkoly(ukoly, soubor);

    return 0;
}
```

- [ ] **Step 3: Ověř, že refaktor nic nerozbil**

Run: `g++ todolist.cpp -o todolist.exe`
Expected: žádné chyby ani varování.

Run: `./todolist.exe`
Expected: program doběhne bez pádu (chování je stejné jako před refaktorem — `main()` zatím jen načte a uloží soubor).

- [ ] **Step 4: Commit**

```bash
git add todolist.hpp todolist.cpp
git commit -m "refactor: extract todolist logic into header-only todolist.hpp"
```

---

### Task 2: TDD pro parsování příkazů (rozeberPrikaz)

**Files:**
- Modify: `todolist.hpp` (přidat `TypPrikazu`, `Prikaz`, `rozeberPrikaz`)
- Create: `test_todolist.cpp`

- [ ] **Step 1: Napiš `test_todolist.cpp` s testy pro `rozeberPrikaz`, který v hpp ještě neexistuje**

```cpp
#include "todolist.hpp"
#include <cassert>
#include <iostream>

void test_konec() {
    Prikaz p = rozeberPrikaz("q");
    assert(p.typ == TypPrikazu::Konec);
}

void test_vypsat() {
    Prikaz p = rozeberPrikaz("v");
    assert(p.typ == TypPrikazu::Vypsat);
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

int main() {
    test_konec();
    test_vypsat();
    test_pridat_s_popisem();
    test_pridat_bez_popisu();
    test_oznacit_platne_id();
    test_odebrat_platne_id();
    test_oznacit_chybne_id();
    test_oznacit_chybejici_id();
    test_neznamy_prikaz();

    std::cout << "Vsechny testy prosly.\n";
    return 0;
}
```

- [ ] **Step 2: Ověř, že testy bez implementace selžou (chyba kompilace)**

Run: `g++ test_todolist.cpp -o test_todolist.exe`
Expected: FAIL — `error: 'TypPrikazu' was not declared` (nebo obdobná chyba, protože `rozeberPrikaz`/`TypPrikazu`/`Prikaz` v `todolist.hpp` ještě neexistují).

- [ ] **Step 3: Přidej do `todolist.hpp` typy a implementaci `rozeberPrikaz` (na konec souboru, za `oznacitUkolDokonceny`)**

```cpp
enum class TypPrikazu { Pridat, Vypsat, Oznacit, Odebrat, Konec, Neznamy };

struct Prikaz {
    TypPrikazu typ = TypPrikazu::Neznamy;
    std::string popis;
    int id = -1;
};

inline Prikaz rozeberPrikaz(const std::string& radek) {
    Prikaz prikaz;
    std::istringstream ss(radek);
    std::string token;
    ss >> token;

    if (token == "q") {
        prikaz.typ = TypPrikazu::Konec;
    } else if (token == "v") {
        prikaz.typ = TypPrikazu::Vypsat;
    } else if (token == "p") {
        prikaz.typ = TypPrikazu::Pridat;
        std::string zbytek;
        std::getline(ss, zbytek);
        size_t start = zbytek.find_first_not_of(' ');
        prikaz.popis = (start == std::string::npos) ? "" : zbytek.substr(start);
    } else if (token == "o" || token == "r") {
        prikaz.typ = (token == "o") ? TypPrikazu::Oznacit : TypPrikazu::Odebrat;
        std::string idStr;
        if (ss >> idStr) {
            try {
                prikaz.id = std::stoi(idStr);
            } catch (...) {
                prikaz.typ = TypPrikazu::Neznamy;
            }
        } else {
            prikaz.typ = TypPrikazu::Neznamy;
        }
    } else {
        prikaz.typ = TypPrikazu::Neznamy;
    }

    return prikaz;
}
```

- [ ] **Step 4: Ověř, že testy teď projdou**

Run: `g++ test_todolist.cpp -o test_todolist.exe && ./test_todolist.exe`
Expected: PASS — vypíše se `Vsechny testy prosly.` a program vrátí návratový kód 0 (žádný `assert` nespadne).

- [ ] **Step 5: Commit**

```bash
git add todolist.hpp test_todolist.cpp
git commit -m "feat: add rozeberPrikaz command parser with assert-based tests"
```

---

### Task 3: Interaktivní smyčka v main()

**Files:**
- Modify: `todolist.cpp` (celý `main()`)

- [ ] **Step 1: Nahraď `main()` v `todolist.cpp` interaktivní smyčkou**

```cpp
#include "todolist.hpp"

void vypisNapovedu() {
    std::cout << "\nPrikazy:\n"
              << "  p <popis>  - pridat ukol\n"
              << "  v          - vypsat ukoly\n"
              << "  o <id>     - oznacit ukol jako hotovy\n"
              << "  r <id>     - odebrat ukol\n"
              << "  q          - konec\n"
              << "> ";
}

int main() {
    const std::string soubor = "ukoly.txt";
    std::vector<Task> ukoly = nactiUkoly(soubor);

    std::string radek;
    while (true) {
        vypisNapovedu();
        if (!std::getline(std::cin, radek)) break;

        Prikaz prikaz = rozeberPrikaz(radek);
        if (prikaz.typ == TypPrikazu::Konec) break;

        switch (prikaz.typ) {
            case TypPrikazu::Vypsat:
                vytiskniUkoly(ukoly);
                break;
            case TypPrikazu::Pridat:
                pridatUkol(ukoly, prikaz.popis);
                break;
            case TypPrikazu::Oznacit:
                if (!oznacitUkolDokonceny(ukoly, prikaz.id)) {
                    std::cout << "Ukol s ID " << prikaz.id << " nenalezen.\n";
                }
                break;
            case TypPrikazu::Odebrat:
                if (!odebratUkol(ukoly, prikaz.id)) {
                    std::cout << "Ukol s ID " << prikaz.id << " nenalezen.\n";
                }
                break;
            case TypPrikazu::Konec:
                break;
            default:
                std::cout << "Neznamy prikaz.\n";
                break;
        }
    }

    ulozUkoly(ukoly, soubor);
    return 0;
}
```

- [ ] **Step 2: Zkompiluj**

Run: `g++ todolist.cpp -o todolist.exe`
Expected: žádné chyby ani varování.

- [ ] **Step 3: Ruční test golden path**

Run: `printf 'p Udelat si obed\np Pridat si ukol do seznamu\no 2\nv\nr 1\nv\nq\n' | ./todolist.exe`

Expected výstup obsahuje po `v` (druhé volání) jen úkol s ID 2 (`Pridat si ukol do seznamu`), označený `Dokonceno: Ano` — úkol s ID 1 byl odebrán.

- [ ] **Step 4: Ruční test chybových stavů**

Run: `printf 'o 99\nr 99\nblah\nq\n' | ./todolist.exe`

Expected výstup obsahuje třikrát chybovou hlášku: dvakrát `Ukol s ID 99 nenalezen.` a jednou `Neznamy prikaz.` — program neshodí (spadne až na `q`).

- [ ] **Step 5: Ověř perzistenci do `ukoly.txt`**

Run: `rm -f ukoly.txt && printf 'p Test ukol\nq\n' | ./todolist.exe && cat ukoly.txt`

Expected: soubor `ukoly.txt` obsahuje řádek `1;Test ukol;0`.

- [ ] **Step 6: Commit**

```bash
git add todolist.cpp
git commit -m "feat: add interactive command loop to main()"
```
