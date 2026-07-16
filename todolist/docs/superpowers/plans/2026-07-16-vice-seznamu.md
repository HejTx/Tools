# Více seznamů úkolů (Multiple Task Lists) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Multiple named task lists with stable IDs, inline completion percentages, and fast switching — all inside the single encrypted `ukoly.txt` under one password.

**Architecture:** New `Seznam` struct (id, nazev, ukoly) and `StavSeznamu` (vector of lists + active ID) in header-only `todolist.hpp`. New plaintext format `@aktivni;<id>` + `#seznam;<id>;<nazev>` sections reusing existing `serializujUkoly`/`parsujUkoly`; legacy single-list content auto-migrates to one list `Ukoly`. Command parser gains `n/v/j/d`; screen gains a list bar with percentages; `main()` operates on the active list.

**Tech Stack:** C++17, libsodium (unchanged), plain-assert tests in `test_todolist.cpp`.

**Context:** User wants several named lists (e.g. "Nakup", "Ukoly EQ tyden") without typing long names — names for display, numeric IDs for commands. `d` deletes the active list, `d <id>` a specific one, no confirmation (per user). Spec approved and committed: `docs/superpowers/specs/2026-07-16-vice-seznamu-design.md`.

**Build/test commands (used in every task):**
```bash
g++ test_todolist.cpp -o test_todolist.exe -lsodium && ./test_todolist.exe
g++ todolist.cpp -o todolist.exe -lsodium
```

---

### Task 0: Save this plan into the repo

- [ ] Copy this plan to `docs/superpowers/plans/2026-07-16-vice-seznamu.md`, commit:
```bash
git add docs/superpowers/plans/2026-07-16-vice-seznamu.md
git commit -m "docs: add implementation plan for multiple task lists"
```

---

### Task 1: Percentage formatting

**Files:** Modify `todolist.hpp` (after `Task` struct), Test `test_todolist.cpp`

- [ ] **Step 1: Write failing tests** (add to `test_todolist.cpp`, register in `main()`):

```cpp
void test_procenta() {
    assert(formatujProcenta({}) == "0.0%");
    assert(formatujProcenta({{1, "a", true}, {2, "b", false}}) == "50.0%");
    assert(formatujProcenta({{1, "a", true}, {2, "b", false}, {3, "c", false}}) == "33.3%");
    assert(formatujProcenta({{1, "a", true}, {2, "b", true}, {3, "c", false}}) == "66.7%");
    assert(formatujProcenta({{1, "a", true}}) == "100.0%");
}
```

- [ ] **Step 2: Run** `g++ test_todolist.cpp -o test_todolist.exe -lsodium` — expected: FAIL (compile error, `formatujProcenta` undeclared)

- [ ] **Step 3: Implement** in `todolist.hpp` (below `Task`):

```cpp
// Podíl hotových úkolů jako "50.0%"; prázdný seznam = "0.0%".
inline std::string formatujProcenta(const std::vector<Task>& ukoly) {
    int hotovo = 0;
    for (const auto& ukol : ukoly) {
        if (ukol.done) ++hotovo;
    }
    double procenta = ukoly.empty() ? 0.0 : 100.0 * hotovo / ukoly.size();
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f%%", procenta);
    return buf;
}
```

- [ ] **Step 4: Run tests** — expected: `Vsechny testy prosly.`
- [ ] **Step 5: Commit** `git add todolist.hpp test_todolist.cpp && git commit -m "feat: add completion percentage formatting"`

---

### Task 2: `Seznam`/`StavSeznamu` + serialization, parsing, migration

**Files:** Modify `todolist.hpp` (after `parsujUkoly`), Test `test_todolist.cpp`

- [ ] **Step 1: Write failing tests:**

```cpp
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
```

- [ ] **Step 2: Compile** — expected: FAIL (`StavSeznamu` undeclared)

- [ ] **Step 3: Implement** in `todolist.hpp` after `parsujUkoly`:

```cpp
struct Seznam {
    int id;
    std::string nazev;
    std::vector<Task> ukoly;
};

struct StavSeznamu {
    std::vector<Seznam> seznamy;
    int aktivniId = 1;
};

inline Seznam* najdiSeznam(std::vector<Seznam>& seznamy, int id) {
    for (auto& seznam : seznamy) {
        if (seznam.id == id) return &seznam;
    }
    return nullptr;
}

inline const Seznam* najdiSeznam(const std::vector<Seznam>& seznamy, int id) {
    for (const auto& seznam : seznamy) {
        if (seznam.id == id) return &seznam;
    }
    return nullptr;
}

inline std::string serializujSeznamy(const StavSeznamu& stav) {
    std::ostringstream out;
    out << "@aktivni;" << stav.aktivniId << "\n";
    for (const auto& seznam : stav.seznamy) {
        out << "#seznam;" << seznam.id << ";" << seznam.nazev << "\n"
            << serializujUkoly(seznam.ukoly);
    }
    return out.str();
}

// Rozparsuje nový formát; obsah bez hlavičky @aktivni je starý jednoseznamový
// formát a zmigruje se na jeden seznam "Ukoly". Vždy vrátí aspoň jeden seznam
// a aktivniId ukazující na existující seznam.
inline StavSeznamu parsujSeznamy(const std::string& obsah) {
    StavSeznamu stav;

    if (obsah.rfind("@aktivni;", 0) != 0) {
        stav.seznamy.push_back({1, "Ukoly", parsujUkoly(obsah)});
        return stav;
    }

    std::istringstream in(obsah);
    std::string line;
    std::getline(in, line);
    try {
        stav.aktivniId = std::stoi(line.substr(std::string("@aktivni;").size()));
    } catch (...) {}

    std::string bufferUkolu;
    auto uzavriSeznam = [&]() {
        if (!stav.seznamy.empty()) {
            stav.seznamy.back().ukoly = parsujUkoly(bufferUkolu);
        }
        bufferUkolu.clear();
    };

    while (std::getline(in, line)) {
        if (line.rfind("#seznam;", 0) == 0) {
            uzavriSeznam();
            std::istringstream ss(line);
            std::string prefix, idStr, nazev;
            std::getline(ss, prefix, ';');
            std::getline(ss, idStr, ';');
            std::getline(ss, nazev);
            Seznam seznam;
            try {
                seznam.id = std::stoi(idStr);
            } catch (...) {
                seznam.id = stav.seznamy.empty() ? 1 : stav.seznamy.back().id + 1;
            }
            seznam.nazev = nazev;
            stav.seznamy.push_back(std::move(seznam));
        } else {
            bufferUkolu += line;
            bufferUkolu += "\n";
        }
    }
    uzavriSeznam();

    if (stav.seznamy.empty()) {
        stav.seznamy.push_back({1, "Ukoly", {}});
    }
    if (najdiSeznam(stav.seznamy, stav.aktivniId) == nullptr) {
        stav.aktivniId = stav.seznamy.front().id;
    }
    return stav;
}
```

(`Seznam` needs a default `id`? No — parse path sets it in both branches; aggregate init used elsewhere. Add `int id = 0;` only if compiler warns.)

- [ ] **Step 4: Run tests** — expected: PASS
- [ ] **Step 5: Commit** `git commit -am "feat: multi-list data model, serialization and legacy migration"`

---

### Task 3: List operations (create/switch/rename/delete)

**Files:** Modify `todolist.hpp` (after `parsujSeznamy`), Test `test_todolist.cpp`

- [ ] **Step 1: Write failing tests:**

```cpp
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
```

- [ ] **Step 2: Compile** — expected: FAIL (`pridatSeznam` undeclared)

- [ ] **Step 3: Implement:**

```cpp
// Založí seznam a rovnou na něj přepne. Vrací nové ID.
inline int pridatSeznam(StavSeznamu& stav, const std::string& nazev) {
    int noveId = stav.seznamy.empty() ? 1 : stav.seznamy.back().id + 1;
    stav.seznamy.push_back({noveId, nazev, {}});
    stav.aktivniId = noveId;
    return noveId;
}

inline bool vybratSeznam(StavSeznamu& stav, int id) {
    if (najdiSeznam(stav.seznamy, id) == nullptr) return false;
    stav.aktivniId = id;
    return true;
}

inline bool prejmenovatSeznam(std::vector<Seznam>& seznamy, int id, const std::string& nazev) {
    Seznam* seznam = najdiSeznam(seznamy, id);
    if (!seznam) return false;
    seznam->nazev = nazev;
    return true;
}

// Smaže seznam podle ID. Po smazání aktivního se aktivním stane první
// zbývající; po smazání posledního vznikne čerstvý prázdný seznam "Ukoly".
inline bool smazatSeznam(StavSeznamu& stav, int id) {
    auto it = std::find_if(stav.seznamy.begin(), stav.seznamy.end(),
                           [id](const Seznam& seznam) { return seznam.id == id; });
    if (it == stav.seznamy.end()) return false;
    stav.seznamy.erase(it);

    if (stav.seznamy.empty()) {
        stav.seznamy.push_back({1, "Ukoly", {}});
        stav.aktivniId = 1;
    } else if (najdiSeznam(stav.seznamy, stav.aktivniId) == nullptr) {
        stav.aktivniId = stav.seznamy.front().id;
    }
    return true;
}
```

- [ ] **Step 4: Run tests** — expected: PASS
- [ ] **Step 5: Commit** `git commit -am "feat: list operations create/switch/rename/delete"`

---

### Task 4: Command parser extensions (`n`, `v`, `j`, `d`)

**Files:** Modify `todolist.hpp` (`TypPrikazu`, `rozeberPrikaz`), Test `test_todolist.cpp`

- [ ] **Step 1: Write failing tests:**

```cpp
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
}
```

- [ ] **Step 2: Compile** — expected: FAIL (`NovySeznam` not a member)

- [ ] **Step 3: Implement.** Extend enum:

```cpp
enum class TypPrikazu { Pridat, Oznacit, Odebrat, Konec, Neznamy, Ulozit,
                        NovySeznam, VybratSeznam, PrejmenovatSeznam, SmazatSeznam };
```

Extract two helpers above `rozeberPrikaz` (DRY — reuse them in existing `p` and `o|r` branches too):

```cpp
// Zbytek řádku bez úvodních mezer (popis úkolu / název seznamu).
inline std::string zbytekRadku(std::istringstream& ss) {
    std::string zbytek;
    std::getline(ss, zbytek);
    size_t start = zbytek.find_first_not_of(' ');
    return (start == std::string::npos) ? "" : zbytek.substr(start);
}

// Načte číselný argument do prikaz.id; při chybě nastaví Neznamy a vrátí false.
inline bool nactiIdArgument(std::istringstream& ss, Prikaz& prikaz) {
    std::string idStr;
    if (!(ss >> idStr)) {
        prikaz.typ = TypPrikazu::Neznamy;
        return false;
    }
    try {
        prikaz.id = std::stoi(idStr);
        return true;
    } catch (...) {
        prikaz.typ = TypPrikazu::Neznamy;
        return false;
    }
}
```

New branches in `rozeberPrikaz` (before the final `else`); rewrite `p` branch as `prikaz.popis = zbytekRadku(ss);` and `o|r` branch as `nactiIdArgument(ss, prikaz);`:

```cpp
} else if (token == "n") {
    prikaz.typ = TypPrikazu::NovySeznam;
    prikaz.popis = zbytekRadku(ss);
} else if (token == "v") {
    prikaz.typ = TypPrikazu::VybratSeznam;
    nactiIdArgument(ss, prikaz);
} else if (token == "j") {
    prikaz.typ = TypPrikazu::PrejmenovatSeznam;
    if (nactiIdArgument(ss, prikaz)) {
        prikaz.popis = zbytekRadku(ss);
    }
} else if (token == "d") {
    prikaz.typ = TypPrikazu::SmazatSeznam;
    std::string idStr;
    if (ss >> idStr) {
        try {
            prikaz.id = std::stoi(idStr);
        } catch (...) {
            prikaz.typ = TypPrikazu::Neznamy;
        }
    }
    // bez argumentu: id zůstává -1 = smazat aktivní seznam
}
```

- [ ] **Step 4: Run tests** — expected: PASS (including untouched `test_vypsat_je_neznamy` — "v" without ID stays `Neznamy`)
- [ ] **Step 5: Commit** `git commit -am "feat: parse list commands n/v/j/d"`

---

### Task 5: Rendering — list bar + new screen layout

**Files:** Modify `todolist.hpp` (`vykresliObrazovku` + new `vytiskniSeznamy`), Test `test_todolist.cpp` (update 3 existing `test_vykresli_*` tests, add list-bar test)

- [ ] **Step 1: Write/replace failing tests.** New test:

```cpp
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
```

Replace the three existing screen tests with the new signature and layout:

```cpp
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
        "Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q\n"
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
        "Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q\n"
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
        "Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q\n"
        "> ");
}
```

- [ ] **Step 2: Compile** — expected: FAIL (`vytiskniSeznamy` undeclared / signature mismatch)

- [ ] **Step 3: Implement.** Replace `vykresliObrazovku` in `todolist.hpp`:

```cpp
inline void vytiskniSeznamy(std::ostream& out, const StavSeznamu& stav) {
    out << "Seznamy: ";
    for (size_t i = 0; i < stav.seznamy.size(); ++i) {
        const Seznam& seznam = stav.seznamy[i];
        if (i > 0) out << " | ";
        std::string polozka = "[" + std::to_string(seznam.id) + "] " + seznam.nazev
                              + " (" + formatujProcenta(seznam.ukoly) + ")";
        if (seznam.id == stav.aktivniId) {
            out << "\033[1m>" << polozka << "<\033[0m";
        } else {
            out << polozka;
        }
    }
    out << "\n";
}

inline void vykresliObrazovku(std::ostream& out,
                              const StavSeznamu& stav,
                              const std::string& zprava) {
    vytiskniSeznamy(out, stav);
    const Seznam* aktivni = najdiSeznam(stav.seznamy, stav.aktivniId);
    out << "=== " << aktivni->nazev << " ===\n";
    if (aktivni->ukoly.empty()) {
        out << "Zadne ukoly.\n";
    } else {
        vytiskniUkoly(out, aktivni->ukoly);
    }
    out << "\n";
    if (!zprava.empty()) {
        out << zprava << "\n\n";
    }
    out << "Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q\n"
        << "> ";
}
```

(Invariant from Task 2/3: `aktivniId` always points at an existing list, so `aktivni` is never null — `main()` maintains it via the operation helpers only.)

- [ ] **Step 4: Run tests** — expected: PASS
- [ ] **Step 5: Commit** `git commit -am "feat: render list bar with percentages and active-list screen"`

---

### Task 6: Save function + `main()` integration

**Files:** Modify `todolist.hpp` (`ulozUkoly` → `ulozSeznamy`), Modify `todolist.cpp` (`main`)

- [ ] **Step 1:** In `todolist.hpp`, rename `ulozUkoly` → `ulozSeznamy` and change the first parameter; body otherwise unchanged (temp file + rename):

```cpp
inline void ulozSeznamy(const StavSeznamu& stav, const std::string& soubor,
                        const std::vector<unsigned char>& klic,
                        const std::array<unsigned char, crypto_pwhash_SALTBYTES>& sul) {
    // ... stejné tělo, jen:
    out << zasifruj(serializujSeznamy(stav), klic, sul);
```

- [ ] **Step 2:** In `todolist.cpp` `main()`: replace `std::vector<Task> ukoly;` with `StavSeznamu stav;`. Loading branches:
  - file missing → `stav = parsujSeznamy("");` (yields one empty `Ukoly` list) — keep the "zaklada se novy" message and `nastavNovyKlic`
  - encrypted → `stav = parsujSeznamy(vysledek->plaintext);` (handles both legacy and new plaintext)
  - plain legacy file → `stav = parsujSeznamy(*obsah);`

  In the loop, resolve the active list each iteration before the switch:

```cpp
Seznam* aktivni = najdiSeznam(stav.seznamy, stav.aktivniId);
```

  `Pridat`/`Oznacit`/`Odebrat` operate on `aktivni->ukoly` (message texts unchanged). `Ulozit` and the final save call `ulozSeznamy(stav, soubor, klic, sul);`. `vykresliObrazovku(std::cout, stav, zprava);`.

  New switch branches:

```cpp
case TypPrikazu::NovySeznam:
    if (prikaz.popis.empty()) {
        zprava = "Nazev nesmi byt prazdny.";
    } else if (prikaz.popis.find(';') != std::string::npos) {
        zprava = "Nazev nesmi obsahovat znak ';'.";
    } else {
        pridatSeznam(stav, prikaz.popis);
        zprava = "Seznam '" + prikaz.popis + "' zalozen.";
    }
    break;
case TypPrikazu::VybratSeznam:
    if (vybratSeznam(stav, prikaz.id)) {
        zprava = "Prepnuto na seznam '"
                 + najdiSeznam(stav.seznamy, stav.aktivniId)->nazev + "'.";
    } else {
        zprava = "Seznam s ID " + std::to_string(prikaz.id) + " nenalezen.";
    }
    break;
case TypPrikazu::PrejmenovatSeznam:
    if (prikaz.popis.empty()) {
        zprava = "Nazev nesmi byt prazdny.";
    } else if (prikaz.popis.find(';') != std::string::npos) {
        zprava = "Nazev nesmi obsahovat znak ';'.";
    } else if (prejmenovatSeznam(stav.seznamy, prikaz.id, prikaz.popis)) {
        zprava = "Seznam prejmenovan na '" + prikaz.popis + "'.";
    } else {
        zprava = "Seznam s ID " + std::to_string(prikaz.id) + " nenalezen.";
    }
    break;
case TypPrikazu::SmazatSeznam: {
    int cil = (prikaz.id == -1) ? stav.aktivniId : prikaz.id;
    const Seznam* mazany = najdiSeznam(stav.seznamy, cil);
    if (mazany) {
        std::string nazev = mazany->nazev;
        smazatSeznam(stav, cil);
        zprava = "Seznam '" + nazev + "' smazan.";
    } else {
        zprava = "Seznam s ID " + std::to_string(cil) + " nenalezen.";
    }
    break;
}
```

- [ ] **Step 3: Build both binaries and run tests:**
```bash
g++ todolist.cpp -o todolist.exe -lsodium
g++ test_todolist.cpp -o test_todolist.exe -lsodium && ./test_todolist.exe
```
Expected: clean build, `Vsechny testy prosly.`

- [ ] **Step 4: Commit** `git commit -am "feat: multi-list support in main loop and encrypted save"`

---

### Task 7: End-to-end verification (scripted, in scratchpad dir)

The binary uses relative `ukoly.txt`, so run from the scratchpad. `prectiHeslo` reads plainly when stdin is a pipe.

- [ ] **Fresh start + list workflow:**
```bash
cd <scratchpad>/e2e && rm -f ukoly.txt
printf 'heslo\nheslo\np mleko\no 1\nn Prace\np report\nq\n' | /home/mgardea/todolist/todolist.exe
```
Expected in output: list bar progressing to `[1] Ukoly (100.0%) | \033[1m>[2] Prace (0.0%)<\033[0m`, final `Ukoly ulozeny. Nashledanou.`

- [ ] **Reopen — remembers active list, switching works:**
```bash
printf 'heslo\nv 1\nq\n' | /home/mgardea/todolist/todolist.exe
```
Expected: first screen shows `>[2] Prace<` active (persisted `@aktivni`), then `Prepnuto na seznam 'Ukoly'.`

- [ ] **Delete active + last-list recreation:**
```bash
printf 'heslo\nd\nd\nq\n' | /home/mgardea/todolist/todolist.exe
```
Expected: after both deletes a fresh empty `Ukoly` list exists (`Seznamy: >[1] Ukoly (0.0%)<`).

- [ ] **Legacy migration:**
```bash
printf '1;stary ukol;0\n' > ukoly.txt
printf 'heslo\nheslo\nq\n' | /home/mgardea/todolist/todolist.exe
```
Expected: message about nesifrovany format, screen shows `>[1] Ukoly (0.0%)<` with `ID: 1, Popis: stary ukol`.

- [ ] Also invoke the project `verify` skill if applicable, then `superpowers:requesting-code-review` per workflow, and finish with `superpowers:finishing-a-development-branch`.

---

## Verification summary

- Unit: `g++ test_todolist.cpp -o test_todolist.exe -lsodium && ./test_todolist.exe` → `Vsechny testy prosly.`
- Build: `g++ todolist.cpp -o todolist.exe -lsodium` → no errors.
- E2E: scripted piped-stdin runs above (fresh file, reopen, delete-last, legacy migration).
- The user's real `ukoly.txt` (encrypted, single-list) migrates on first save — `parsujSeznamy` handles the decrypted legacy plaintext.

## Notes

- Spec: `docs/superpowers/specs/2026-07-16-vice-seznamu-design.md` (committed, `3e7e3ea`).
- Existing test `test_vypsat_je_neznamy` ("v" alone → `Neznamy`) intentionally still holds.
- No delete confirmation, per user decision recorded in spec.
