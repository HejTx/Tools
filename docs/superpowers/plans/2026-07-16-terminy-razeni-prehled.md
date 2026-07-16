# Termíny, řazení a přehled „Vše" Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Optional due dates (`t <id> <dd/mm/yy>`), a `z` sort-mode cycle (ID ↔ termin), and a virtual list 0 aggregating all tasks with compound `<seznam>.<ukol>` addressing.

**Architecture:** `Task` gains `termin`; file format gains a 4th field and an `@razeni` header (both backward compatible). Sorting is render-only (`serazeneUkoly`, `sestavPrehled`). `aktivniId == 0` becomes a valid virtual state preserved by `vybratSeznam`/`smazatSeznam`/`parsujSeznamy`; `main()` resolves each task command's target list through one shared helper (compound ID > active list > error in overview).

**Tech Stack:** C++17, libsodium, make. Spec: `docs/superpowers/specs/2026-07-16-terminy-razeni-prehled-design.md`

**Test command:** `make test` (note: the save test prints one expected stderr line)

---

### Task 1: `Task.termin` — validation, key, serialization, display

**Files:** Modify `todolist.hpp`, Test `test_todolist.cpp`

- [ ] **Step 1: Failing tests** (register each new test in `main()` near its siblings):

```cpp
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
}

void test_serializace_terminu() {
    std::vector<Task> ukoly = {{1, "a", false, "18/07/26"}, {2, "b", true}};
    assert(serializujUkoly(ukoly) == "1;a;0;18/07/26\n2;b;1;\n");
    std::vector<Task> zpet = parsujUkoly(serializujUkoly(ukoly));
    assert(zpet[0].termin == "18/07/26" && zpet[1].termin == "");
    // stary 3-polovy format -> bez terminu
    std::vector<Task> stare = parsujUkoly("1;nakoupit;1\n");
    assert(stare[0].termin == "" && stare[0].done);
}

void test_vytiskni_ukol_s_terminem() {
    std::ostringstream out;
    vytiskniUkol(out, {1, "mleko", false, "18/07/26"});
    assert(out.str() == "ID: 1, Popis: mleko, Dokonceno: Ne, Termin: 18/07/26\n");
    std::ostringstream out2;
    vytiskniUkol(out2, {2, "chleba", false}, "3.");
    assert(out2.str() == "ID: 3.2, Popis: chleba, Dokonceno: Ne\n");
}
```

Update the two existing serialization exact-string tests: `test_serializace_format` expects `"1;nakoupit;1;\n2;uklidit;0;\n"`; `test_serializace_seznamu` expects task lines `"1;mleko;0;\n"`, `"2;chleba;1;\n"` (and, after Task 2, the `@razeni;1` header line).

- [ ] **Step 2:** `make test` — compile FAIL (`jePlatnyTermin` undeclared)

- [ ] **Step 3: Implement** in `todolist.hpp`. Add includes `<cctype>`, `<limits>`. `Task` gains `std::string termin;` (last member). New helpers after `Task`:

```cpp
// Formát dd/mm/yy: číslice + lomítka, den 01-31, měsíc 01-12.
inline bool jePlatnyTermin(const std::string& termin) {
    if (termin.size() != 8 || termin[2] != '/' || termin[5] != '/') return false;
    for (size_t i : {0, 1, 3, 4, 6, 7}) {
        if (!std::isdigit(static_cast<unsigned char>(termin[i]))) return false;
    }
    int den = std::stoi(termin.substr(0, 2));
    int mesic = std::stoi(termin.substr(3, 2));
    return den >= 1 && den <= 31 && mesic >= 1 && mesic <= 12;
}

// Porovnávací klíč rr*10000+mm*100+dd; bez termínu = max (řadí se nakonec).
inline int klicTerminu(const std::string& termin) {
    if (termin.empty()) return std::numeric_limits<int>::max();
    int den = std::stoi(termin.substr(0, 2));
    int mesic = std::stoi(termin.substr(3, 2));
    int rok = std::stoi(termin.substr(6, 2));
    return rok * 10000 + mesic * 100 + den;
}
```

`serializujUkoly` line becomes `out << ukol.id << ";" << ukol.description << ";" << ukol.done << ";" << ukol.termin << "\n";`. In `parsujUkoly` add `std::string terminStr;` + `std::getline(ss, terminStr, ';');` after `doneStr` and `ukol.termin = terminStr;` (an old 3-field line leaves it empty). `vytiskniUkol` gains `const std::string& prefixId = ""` (3rd param), prints `"ID: " << prefixId << ukol.id ...` and appends `", Termin: " << ukol.termin` when non-empty (inside the grey span for done tasks).

- [ ] **Step 4:** `make test` — PASS. **Step 5:** `git commit -am "feat: tasks carry optional due date (termin)"`

---

### Task 2: `razeni` state + sort/overview helpers + `formatujProcenta(int,int)`

**Files:** Modify `todolist.hpp`, Test `test_todolist.cpp`

- [ ] **Step 1: Failing tests:**

```cpp
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
}
```

- [ ] **Step 2:** `make test` — compile FAIL

- [ ] **Step 3: Implement.** `StavSeznamu` gains `int razeni = 1;`. `serializujSeznamy` writes `out << "@razeni;" << stav.razeni << "\n";` right after the `@aktivni` line. In `parsujSeznamy`'s while-loop add before the `#seznam` branch:

```cpp
        if (line.rfind("@razeni;", 0) == 0) {
            try {
                stav.razeni = std::stoi(line.substr(std::string("@razeni;").size()));
            } catch (...) {}
            continue;
        }
```

and in the normalization tail add `if (stav.razeni != 2) stav.razeni = 1;`. Refactor `formatujProcenta` into a two-int core plus the existing vector wrapper:

```cpp
inline std::string formatujProcenta(int hotovo, int celkem) {
    double procenta = (celkem == 0) ? 0.0 : 100.0 * hotovo / celkem;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f%%", procenta);
    return buf;
}

inline std::string formatujProcenta(const std::vector<Task>& ukoly) {
    int hotovo = 0;
    for (const auto& ukol : ukoly) {
        if (ukol.done) ++hotovo;
    }
    return formatujProcenta(hotovo, static_cast<int>(ukoly.size()));
}
```

New helpers (after `smazatSeznam`):

```cpp
// Kopie úkolů v zobrazovacím pořadí (1 = pořadí dat/ID, 2 = podle termínu).
inline std::vector<Task> serazeneUkoly(const std::vector<Task>& ukoly, int razeni) {
    std::vector<Task> vysledek = ukoly;
    if (razeni == 2) {
        std::stable_sort(vysledek.begin(), vysledek.end(), [](const Task& a, const Task& b) {
            if (klicTerminu(a.termin) != klicTerminu(b.termin)) {
                return klicTerminu(a.termin) < klicTerminu(b.termin);
            }
            return a.id < b.id;
        });
    }
    return vysledek;
}

struct PolozkaPrehledu {
    int seznamId;
    Task ukol;
};

// Všechny úkoly napříč seznamy v zobrazovacím pořadí přehledu „Vše".
inline std::vector<PolozkaPrehledu> sestavPrehled(const StavSeznamu& stav) {
    std::vector<PolozkaPrehledu> polozky;
    for (const auto& seznam : stav.seznamy) {
        for (const auto& ukol : seznam.ukoly) {
            polozky.push_back({seznam.id, ukol});
        }
    }
    if (stav.razeni == 2) {
        std::stable_sort(polozky.begin(), polozky.end(),
                         [](const PolozkaPrehledu& a, const PolozkaPrehledu& b) {
            if (klicTerminu(a.ukol.termin) != klicTerminu(b.ukol.termin)) {
                return klicTerminu(a.ukol.termin) < klicTerminu(b.ukol.termin);
            }
            if (a.seznamId != b.seznamId) return a.seznamId < b.seznamId;
            return a.ukol.id < b.ukol.id;
        });
    }
    return polozky;
}
```

Update `test_serializace_seznamu`'s expected string to include `"@razeni;1\n"` after `"@aktivni;2\n"`.

- [ ] **Step 4:** `make test` — PASS. **Step 5:** `git commit -am "feat: sort mode state, sorted views and overview builder"`

---

### Task 3: Parser — compound IDs, `t`, `z`

**Files:** Modify `todolist.hpp` (`TypPrikazu`, `Prikaz`, new `nactiUkolId`, `rozeberPrikaz`), Test `test_todolist.cpp`

- [ ] **Step 1: Failing tests:**

```cpp
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
```

- [ ] **Step 2:** `make test` — compile FAIL

- [ ] **Step 3: Implement.** Enum gains `Termin, PrepnoutRazeni`. `Prikaz` gains `int seznamUkolu = -1;`. New helper below `nactiIdArgument`:

```cpp
// Načte ID úkolu, volitelně složené <seznam>.<ukol> (seznam -> prikaz.seznamUkolu).
inline bool nactiUkolId(std::istringstream& ss, Prikaz& prikaz) {
    std::string token;
    if (!(ss >> token)) {
        prikaz.typ = TypPrikazu::Neznamy;
        return false;
    }
    size_t tecka = token.find('.');
    try {
        if (tecka != std::string::npos) {
            if (tecka == 0 || tecka + 1 >= token.size()) throw std::invalid_argument("id");
            prikaz.seznamUkolu = std::stoi(token.substr(0, tecka));
            prikaz.id = std::stoi(token.substr(tecka + 1));
        } else {
            prikaz.id = std::stoi(token);
        }
        return true;
    } catch (...) {
        prikaz.typ = TypPrikazu::Neznamy;
        return false;
    }
}
```

In `rozeberPrikaz`: `o|r` branch and `e` branch and `m`'s first argument switch from `nactiIdArgument(ss, prikaz, prikaz.id)` to `nactiUkolId(ss, prikaz)`. New branches:

```cpp
    } else if (token == "t") {
        prikaz.typ = TypPrikazu::Termin;
        if (nactiUkolId(ss, prikaz)) {
            prikaz.popis = zbytekRadku(ss);
        }
    } else if (token == "z") {
        prikaz.typ = TypPrikazu::PrepnoutRazeni;
```

(`v`/`j`/`d` keep plain `nactiIdArgument` — they take list IDs. Note `"2.x"`: `std::stoi("x")` throws, so the whole token is rejected; `"2.3junk"` parses as 2.3 — same prefix-parse quirk as plain IDs.)

- [ ] **Step 4:** `make test` — PASS. **Step 5:** `git commit -am "feat: parse compound task IDs, t and z commands"`

---

### Task 4: Operations — `nastavTermin`, `presunUkol` with source, list-0 state

**Files:** Modify `todolist.hpp`, Test `test_todolist.cpp`

- [ ] **Step 1: Failing tests** (plus update the existing `test_presun_ukolu` calls to the new 4-arg signature — first arg after `stav` is the source list ID `1`; the "cil = aktivni" comment becomes "cil == zdroj"):

```cpp
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
    assert(smazatSeznam(stav, 2) && stav.aktivniId == 0);   // i po poslednim
    assert(stav.seznamy.size() == 1 && stav.seznamy[0].nazev == "Ukoly");
    // @aktivni;0 prezije round-trip
    StavSeznamu zpet = parsujSeznamy(serializujSeznamy(stav));
    assert(zpet.aktivniId == 0);
}
```

- [ ] **Step 2:** `make test` — compile FAIL

- [ ] **Step 3: Implement.**

```cpp
// Nastaví (či prázdným řetězcem smaže) termín úkolu.
inline bool nastavTermin(std::vector<Task>& ukoly, int id, const std::string& termin) {
    for (auto& ukol : ukoly) {
        if (ukol.id == id) {
            ukol.termin = termin;
            return true;
        }
    }
    return false;
}
```

`presunUkol` gains the source parameter (replaces the active-list lookup):

```cpp
// Přesune úkol ze zdrojového seznamu do cílového; v cíli dostane nové ID.
// 0 = OK, 1 = úkol (či zdroj) nenalezen, 2 = cíl neexistuje, 3 = cíl == zdroj.
inline int presunUkol(StavSeznamu& stav, int zdrojId, int ukolId, int cilId) {
    Seznam* zdroj = najdiSeznam(stav.seznamy, zdrojId);
    if (!zdroj) return 1;
    auto it = std::find_if(zdroj->ukoly.begin(), zdroj->ukoly.end(),
                           [ukolId](const Task& ukol) { return ukol.id == ukolId; });
    if (it == zdroj->ukoly.end()) return 1;
    if (cilId == zdrojId) return 3;
    Seznam* cil = najdiSeznam(stav.seznamy, cilId);
    if (!cil) return 2;
    Task presouvany = *it;
    zdroj->ukoly.erase(it);
    presouvany.id = cil->ukoly.empty() ? 1 : cil->ukoly.back().id + 1;
    cil->ukoly.push_back(presouvany);
    return 0;
}
```

`vybratSeznam`: first line becomes `if (id != 0 && najdiSeznam(stav.seznamy, id) == nullptr) return false;`. `smazatSeznam` tail preserves 0:

```cpp
    if (stav.seznamy.empty()) {
        stav.seznamy.push_back({1, "Ukoly", {}});
        if (stav.aktivniId != 0) stav.aktivniId = 1;
    } else if (stav.aktivniId != 0 && najdiSeznam(stav.seznamy, stav.aktivniId) == nullptr) {
        stav.aktivniId = stav.seznamy.front().id;
    }
```

`parsujSeznamy` normalization likewise: `if (stav.aktivniId != 0 && najdiSeznam(stav.seznamy, stav.aktivniId) == nullptr)`.

- [ ] **Step 4:** `make test` — PASS. **Step 5:** `git commit -am "feat: termin setter, source-aware move, virtual list 0 state"`

---

### Task 5: Rendering — `[0] Vse`, title with mode, overview body, footer, help

**Files:** Modify `todolist.hpp` (`vytiskniSeznamy`, `vykresliObrazovku`, `vytiskniNapovedu`), Test `test_todolist.cpp`

- [ ] **Step 1: Update/extend expected strings (failing first).**
  - `test_vytiskni_seznamy` expects `"Seznamy: [0] Vse (33.3%) | [1] Nakup (50.0%) | \033[1m>[2] Ukoly EQ tyden (0.0%)<\033[0m\n"`.
  - `test_vytiskni_seznamy_zalamovani` (width 45) expects `"Seznamy: [0] Vse (0.0%) | \033[1m>[1] Alfa (0.0%)<\033[0m\n         [2] Beta (0.0%) | [3] Gama (0.0%)\n"`.
  - `test_vytiskni_seznamy_uzka_obrazovka` (width 10) expects `"Seznamy: [0] Vse (0.0%)\n         \033[1m>[1] Alfa (0.0%)<\033[0m\n         [2] Beta (0.0%)\n"`.
  - The three screen tests: bar line gains the `[0] Vse (…%)` prefix item (0.0% for empty, 50.0% for the done/undone pair); title becomes `"=== Ukoly === (razeni: ID)\n"`; footer becomes the three lines from the spec (`t termin` on line 1, `c uklidit` on line 2, `z razeni` on line 3).
  - `test_vytiskni_napovedu`: task section gains after `e`: `"  t <id> <datum>   Nastavi termin (dd/mm/yy); t <id> bez data termin smaze.\n"`; the `c` line becomes `"  c                Odstrani hotove ukoly (v prehledu 0 ze vsech seznamu).\n"`; the `v` line becomes `"  v <id>           Prepne na seznam podle ID; v 0 = prehled vsech ukolu.\n"`; OSTATNI gains after `u`: `"  z                Prepina razeni: podle ID / podle terminu.\n"`; before the final prompt line insert `"Ukoly lze adresovat i jako <seznam>.<ukol> (napr. o 2.3) - v prehledu 0 je to nutne.\n"` followed by `"\n"`.
  - New overview render test:

```cpp
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
```

- [ ] **Step 2:** `make test` — FAIL (assertions)

- [ ] **Step 3: Implement.** Replace `vytiskniSeznamy` body — build the items list with the pseudo-entry first, then the existing wrap loop over it:

```cpp
inline void vytiskniSeznamy(std::ostream& out, const StavSeznamu& stav, int sirka = 80) {
    int hotovoCelkem = 0;
    int celkem = 0;
    for (const auto& seznam : stav.seznamy) {
        celkem += static_cast<int>(seznam.ukoly.size());
        for (const auto& ukol : seznam.ukoly) {
            if (ukol.done) ++hotovoCelkem;
        }
    }
    std::vector<std::pair<int, std::string>> polozky;
    polozky.push_back({0, "[0] Vse (" + formatujProcenta(hotovoCelkem, celkem) + ")"});
    for (const auto& seznam : stav.seznamy) {
        polozky.push_back({seznam.id, "[" + std::to_string(seznam.id) + "] " + seznam.nazev
                                      + " (" + formatujProcenta(seznam.ukoly) + ")"});
    }

    out << "Seznamy: ";
    int delkaRadku = 9;
    bool prvniNaRadku = true;
    for (const auto& [id, polozka] : polozky) {
        bool aktivni = (id == stav.aktivniId);
        int viditelna = static_cast<int>(polozka.size()) + (aktivni ? 2 : 0);
        if (!prvniNaRadku && delkaRadku + 3 + viditelna > sirka) {
            out << "\n         ";
            delkaRadku = 9;
            prvniNaRadku = true;
        }
        if (!prvniNaRadku) {
            out << " | ";
            delkaRadku += 3;
        }
        if (aktivni) {
            out << "\033[1m>" << polozka << "<\033[0m";
        } else {
            out << polozka;
        }
        delkaRadku += viditelna;
        prvniNaRadku = false;
    }
    out << "\n";
}
```

`vykresliObrazovku` middle section becomes:

```cpp
    const char* rezim = (stav.razeni == 2) ? "termin" : "ID";
    if (stav.aktivniId == 0) {
        out << "=== Vse === (razeni: " << rezim << ")\n";
        std::vector<PolozkaPrehledu> polozky = sestavPrehled(stav);
        if (polozky.empty()) {
            out << "Zadne ukoly.\n";
        } else {
            for (const auto& polozka : polozky) {
                vytiskniUkol(out, polozka.ukol, std::to_string(polozka.seznamId) + ".");
            }
        }
    } else {
        const Seznam* aktivni = najdiSeznam(stav.seznamy, stav.aktivniId);
        out << "=== " << aktivni->nazev << " === (razeni: " << rezim << ")\n";
        std::vector<Task> ukoly = serazeneUkoly(aktivni->ukoly, stav.razeni);
        if (ukoly.empty()) {
            out << "Zadne ukoly.\n";
        } else {
            vytiskniUkoly(out, ukoly);
        }
    }
```

Footer lines become:

```cpp
    out << "\033[90mukol: p pridat · o hotovo · r odebrat · e upravit · m presunout · t termin\n"
           "seznam: n novy · v vybrat · j prejmenovat · d smazat · c uklidit\n"
           "jine: u zpet · z razeni · s ulozit · zh heslo · q konec · h napoveda\033[0m\n"
        << "> ";
```

`vytiskniNapovedu` gets the help changes mirrored from Step 1.

- [ ] **Step 4:** `make test` — PASS. **Step 5:** `git commit -am "feat: render Vse overview, sort-mode title, termin footer/help"`

---

### Task 6: `main()` wiring

**Files:** Modify `todolist.cpp`

- [ ] **Step 1: Implement.** Before the loop (after `predchozi`) add the shared resolver:

```cpp
    // Cílový seznam úkolového příkazu: složené ID > aktivní seznam; v přehledu
    // 0 je složené ID povinné. Při neúspěchu nastaví zprávu a vrátí nullptr.
    auto najdiCilUkolu = [&stav](const Prikaz& prikaz, std::string& zprava) -> Seznam* {
        if (prikaz.seznamUkolu != -1) {
            Seznam* seznam = najdiSeznam(stav.seznamy, prikaz.seznamUkolu);
            if (!seznam) {
                zprava = "Seznam s ID " + std::to_string(prikaz.seznamUkolu) + " nenalezen.";
            }
            return seznam;
        }
        if (stav.aktivniId == 0) {
            zprava = "V prehledu pouzij ID ve tvaru <seznam>.<ukol>, napr. o 2.3.";
            return nullptr;
        }
        return najdiSeznam(stav.seznamy, stav.aktivniId);
    };
```

Switch changes:
- `Pridat`: first check `if (stav.aktivniId == 0) { zprava = "V prehledu nelze pridavat, prepni na seznam."; break; }` (then the existing `;` check and `pridatUkol(najdiSeznam(stav.seznamy, stav.aktivniId)->ukoly, ...)`).
- `Oznacit`/`Odebrat`/`UpravitUkol`: wrap existing logic in `Seznam* cil = najdiCilUkolu(prikaz, zprava); if (cil) { ... }` operating on `cil->ukoly`.
- New `Termin` case:

```cpp
            case TypPrikazu::Termin: {
                Seznam* cil = najdiCilUkolu(prikaz, zprava);
                if (!cil) break;
                if (!prikaz.popis.empty() && !jePlatnyTermin(prikaz.popis)) {
                    zprava = "Neplatny format terminu, pouzij dd/mm/yy.";
                } else if (!nastavTermin(cil->ukoly, prikaz.id, prikaz.popis)) {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                } else {
                    zprava = prikaz.popis.empty() ? "Termin odstranen."
                                                  : "Termin nastaven na " + prikaz.popis + ".";
                }
                break;
            }
```

- `PresunoutUkol`: resolve source via `najdiCilUkolu`; on success compute `int zdrojId = (prikaz.seznamUkolu != -1) ? prikaz.seznamUkolu : stav.aktivniId;` and call `presunUkol(stav, zdrojId, prikaz.id, prikaz.id2)`; the `case 3` message becomes `"Ukol uz je v tomto seznamu."`.
- `VycistitHotove`: `if (stav.aktivniId == 0)` sum `vycistiHotove` over all `stav.seznamy` entries, else existing single-list logic (same messages).
- `SmazatSeznam`: before computing `cil`, `if (prikaz.id == -1 && stav.aktivniId == 0) { zprava = "V prehledu neni aktivni seznam ke smazani."; break; }`.
- `VybratSeznam`: on success `zprava = (stav.aktivniId == 0) ? "Prepnuto na prehled 'Vse'." : "Prepnuto na seznam '" + najdiSeznam(...)->nazev + "'.";`
- New `PrepnoutRazeni` case:

```cpp
            case TypPrikazu::PrepnoutRazeni:
                stav.razeni = (stav.razeni == 2) ? 1 : 2;
                zprava = (stav.razeni == 2) ? "Razeni: podle terminu." : "Razeni: podle ID.";
                break;
```

(`sledovatZmenu` already includes `Termin`/`PrepnoutRazeni`/`PresunoutUkol` — they aren't in its exclusion list — so undo covers them automatically.)

- [ ] **Step 2:** `make all test` — clean build, PASS.
- [ ] **Step 3:** `git commit -am "feat: wire termin, sorting and Vse overview into main loop"`

---

### Task 7: E2E, review, merge

- [ ] E2E in the scratchpad with `XDG_DATA_HOME` isolated: set/clear termin (incl. bad format), `z` cycle + persistence of `@razeni` and `@aktivni;0` across restart, `v 0` in both modes, `o 2.3` from overview and from a regular list, `m 2.3 <id>`, bare `o 3`/`p`/`d` rejections in overview, `c` in overview across lists, undo of a cross-list action.
- [ ] superpowers:requesting-code-review over the branch; fix findings.
- [ ] superpowers:finishing-a-development-branch (user preference: merge to master locally).

## Verification summary

`make test` → `Vsechny testy prosly.`; e2e sweep above; legacy-file compatibility covered by unit tests (3-field lines, missing `@razeni`).
