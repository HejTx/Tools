# Polish Batch Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Nine approved improvements: Makefile, truthful save messages, labeled dim footer, task edit (`e`), clear completed (`c`), move task (`m`), one-level undo (`u`), password change (`zh`), wrapped list bar, fixed data-file location.

**Architecture:** Pure helpers in `todolist.hpp` (TDD, exact-string tests as usual); `main()` wiring in `todolist.cpp`; undo via full-state snapshot compared through `serializujSeznamy`; data path via `std::filesystem` + `XDG_DATA_HOME`/`HOME`.

**Tech Stack:** C++17 (`<filesystem>`), libsodium, make.

**Spec:** `docs/superpowers/specs/2026-07-16-velky-polish-design.md`

**Test command (after Task 1):** `make test`

---

### Task 1: Makefile

**Files:** Create `Makefile`

- [ ] **Step 1:** Create `Makefile`:

```makefile
CXX = g++
LDLIBS = -lsodium

all: todolist.exe

todolist.exe: todolist.cpp todolist.hpp sifrovani.hpp
	$(CXX) todolist.cpp -o $@ $(LDLIBS)

test_todolist.exe: test_todolist.cpp todolist.hpp sifrovani.hpp
	$(CXX) test_todolist.cpp -o $@ $(LDLIBS)

test: test_todolist.exe
	./test_todolist.exe

clean:
	rm -f todolist.exe test_todolist.exe

.PHONY: all test clean
```

- [ ] **Step 2:** Run `make clean && make all test` — expected: both build, `Vsechny testy prosly.`
- [ ] **Step 3:** Commit: `git add Makefile && git commit -m "build: add Makefile"`

---

### Task 2: Truthful save messages

**Files:** Modify `todolist.hpp` (`ulozSeznamy`), `todolist.cpp` (`Ulozit` case + final save), Test `test_todolist.cpp`

- [ ] **Step 1: Failing test** (register in `main()` after `test_neplatne_aktivni_id();`):

```cpp
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

    // selhání: neexistující adresář (hláška na stderr je očekávaná)
    assert(!ulozSeznamy(stav, "neexistujici_adresar/ukoly.txt", klic, sul));
}
```

- [ ] **Step 2:** `make test` — expected: FAIL (`ulozSeznamy` returns void, can't assert)
- [ ] **Step 3:** Change `ulozSeznamy` to return `bool`: `return false;` after each of the three error branches (open fail, write fail, rename fail), `return true;` at the end. In `todolist.cpp`:

```cpp
            case TypPrikazu::Ulozit:
                zprava = ulozSeznamy(stav, soubor, klic, sul)
                             ? "Ukoly ulozeny."
                             : "CHYBA: Ulozeni se nezdarilo, zkus to znovu.";
                break;
```

and the final save:

```cpp
    if (ulozSeznamy(stav, soubor, klic, sul)) {
        std::cout << "\nUkoly ulozeny. Nashledanou.\n";
        return 0;
    }
    std::cout << "\nPOZOR: Ukoly se nepodarilo ulozit!\n";
    return 1;
```

- [ ] **Step 4:** `make all test` — expected: PASS
- [ ] **Step 5:** Commit: `git commit -am "fix: report save failures honestly"`

---

### Task 3: Parser — `e`, `m`, `c`, `u`, `zh` (+ `id2`, helper takes target)

**Files:** Modify `todolist.hpp` (`TypPrikazu`, `Prikaz`, `nactiIdArgument`, `rozeberPrikaz`), Test `test_todolist.cpp`

- [ ] **Step 1: Failing tests:**

```cpp
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
```

- [ ] **Step 2:** `make test` — expected: compile FAIL (`UpravitUkol` not a member)
- [ ] **Step 3:** Enum gets `UpravitUkol, PresunoutUkol, VycistitHotove, Zpet, ZmenaHesla`. `Prikaz` gets `int id2 = -1;`. `nactiIdArgument` signature becomes `(std::istringstream& ss, Prikaz& prikaz, int& cil)` writing to `cil`; existing callers (`o|r`, `v`, `j`) pass `prikaz.id`. New branches:

```cpp
    } else if (token == "e") {
        prikaz.typ = TypPrikazu::UpravitUkol;
        if (nactiIdArgument(ss, prikaz, prikaz.id)) {
            prikaz.popis = zbytekRadku(ss);
        }
    } else if (token == "m") {
        prikaz.typ = TypPrikazu::PresunoutUkol;
        if (nactiIdArgument(ss, prikaz, prikaz.id)) {
            nactiIdArgument(ss, prikaz, prikaz.id2);
        }
    } else if (token == "c") {
        prikaz.typ = TypPrikazu::VycistitHotove;
    } else if (token == "u") {
        prikaz.typ = TypPrikazu::Zpet;
    } else if (token == "zh") {
        prikaz.typ = TypPrikazu::ZmenaHesla;
```

- [ ] **Step 4:** `make test` — expected: PASS
- [ ] **Step 5:** Commit: `git commit -am "feat: parse e/m/c/u/zh commands"`

---

### Task 4: `upravitUkol` + `e` wiring

**Files:** Modify `todolist.hpp` (after `oznacitUkolDokonceny`), `todolist.cpp` (switch), Test `test_todolist.cpp`

- [ ] **Step 1: Failing test:**

```cpp
void test_upravit_ukol() {
    std::vector<Task> ukoly = {{1, "stary", true}};
    assert(upravitUkol(ukoly, 1, "novy"));
    assert(ukoly[0].description == "novy" && ukoly[0].done == true && ukoly[0].id == 1);
    assert(!upravitUkol(ukoly, 99, "x"));
}
```

- [ ] **Step 2:** `make test` — compile FAIL
- [ ] **Step 3:**

```cpp
// Změní jen popis; ID a done zůstávají.
inline bool upravitUkol(std::vector<Task>& ukoly, int id, const std::string& popis) {
    for (auto& ukol : ukoly) {
        if (ukol.id == id) {
            ukol.description = popis;
            return true;
        }
    }
    return false;
}
```

Switch case in `todolist.cpp` (after `Odebrat`):

```cpp
            case TypPrikazu::UpravitUkol:
                if (prikaz.popis.empty()) {
                    zprava = "Popis nesmi byt prazdny.";
                } else if (prikaz.popis.find(';') != std::string::npos) {
                    zprava = "Popis nesmi obsahovat znak ';'.";
                } else if (upravitUkol(najdiSeznam(stav.seznamy, stav.aktivniId)->ukoly,
                                       prikaz.id, prikaz.popis)) {
                    zprava = "Ukol upraven.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
```

- [ ] **Step 4:** `make all test` — PASS. Commit: `git commit -am "feat: e command edits task description"`

---

### Task 5: `vycistiHotove` + `c` wiring

**Files:** Modify `todolist.hpp` (after `upravitUkol`), `todolist.cpp`, Test `test_todolist.cpp`

- [ ] **Step 1: Failing test:**

```cpp
void test_vycisti_hotove() {
    std::vector<Task> ukoly = {{1, "a", true}, {2, "b", false}, {3, "c", true}};
    assert(vycistiHotove(ukoly) == 2);
    assert(ukoly.size() == 1 && ukoly[0].description == "b");
    assert(vycistiHotove(ukoly) == 0);
}
```

- [ ] **Step 2:** `make test` — compile FAIL
- [ ] **Step 3:**

```cpp
// Odstraní hotové úkoly; vrací jejich počet.
inline int vycistiHotove(std::vector<Task>& ukoly) {
    auto it = std::remove_if(ukoly.begin(), ukoly.end(),
                             [](const Task& ukol) { return ukol.done; });
    int pocet = static_cast<int>(ukoly.end() - it);
    ukoly.erase(it, ukoly.end());
    return pocet;
}
```

Switch case:

```cpp
            case TypPrikazu::VycistitHotove: {
                int pocet = vycistiHotove(najdiSeznam(stav.seznamy, stav.aktivniId)->ukoly);
                zprava = (pocet == 0)
                             ? "Zadne hotove ukoly k odstraneni."
                             : "Odstraneno hotovych ukolu: " + std::to_string(pocet) + ".";
                break;
            }
```

- [ ] **Step 4:** `make all test` — PASS. Commit: `git commit -am "feat: c command clears completed tasks"`

---

### Task 6: `presunUkol` + `m` wiring

**Files:** Modify `todolist.hpp` (after `smazatSeznam`), `todolist.cpp`, Test `test_todolist.cpp`

- [ ] **Step 1: Failing test:**

```cpp
void test_presun_ukolu() {
    StavSeznamu stav;
    stav.seznamy = {
        {1, "A", {{1, "prvni", true}, {2, "druhy", false}}},
        {2, "B", {{1, "cizi", false}}},
    };
    stav.aktivniId = 1;
    assert(presunUkol(stav, 1, 2) == 0);
    assert(stav.seznamy[0].ukoly.size() == 1);
    assert(stav.seznamy[1].ukoly.size() == 2);
    assert(stav.seznamy[1].ukoly[1].id == 2);          // nové ID v cíli
    assert(stav.seznamy[1].ukoly[1].description == "prvni");
    assert(stav.seznamy[1].ukoly[1].done == true);     // done se zachová
    assert(presunUkol(stav, 99, 2) == 1);              // úkol nenalezen
    assert(presunUkol(stav, 2, 99) == 2);              // seznam nenalezen
    assert(presunUkol(stav, 2, 1) == 3);               // cíl = aktivní
}
```

- [ ] **Step 2:** `make test` — compile FAIL
- [ ] **Step 3:**

```cpp
// Přesune úkol z aktivního seznamu do cílového; v cíli dostane nové ID.
// 0 = OK, 1 = úkol nenalezen, 2 = cílový seznam neexistuje, 3 = cíl je aktivní.
inline int presunUkol(StavSeznamu& stav, int ukolId, int cilId) {
    if (cilId == stav.aktivniId) return 3;
    Seznam* cil = najdiSeznam(stav.seznamy, cilId);
    if (!cil) return 2;
    Seznam* aktivni = najdiSeznam(stav.seznamy, stav.aktivniId);
    auto it = std::find_if(aktivni->ukoly.begin(), aktivni->ukoly.end(),
                           [ukolId](const Task& ukol) { return ukol.id == ukolId; });
    if (it == aktivni->ukoly.end()) return 1;
    Task presouvany = *it;
    aktivni->ukoly.erase(it);
    presouvany.id = cil->ukoly.empty() ? 1 : cil->ukoly.back().id + 1;
    cil->ukoly.push_back(presouvany);
    return 0;
}
```

Switch case:

```cpp
            case TypPrikazu::PresunoutUkol:
                switch (presunUkol(stav, prikaz.id, prikaz.id2)) {
                    case 0:
                        zprava = "Ukol presunut do seznamu '"
                                 + najdiSeznam(stav.seznamy, prikaz.id2)->nazev + "'.";
                        break;
                    case 1:
                        zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                        break;
                    case 2:
                        zprava = "Seznam s ID " + std::to_string(prikaz.id2) + " nenalezen.";
                        break;
                    default:
                        zprava = "Ukol uz je v aktivnim seznamu.";
                        break;
                }
                break;
```

- [ ] **Step 4:** `make all test` — PASS. Commit: `git commit -am "feat: m command moves task to another list"`

---

### Task 7: `u` — one-level undo (main only)

**Files:** Modify `todolist.cpp`

- [ ] **Step 1:** Before the loop: `std::optional<StavSeznamu> predchozi;`. Inside the loop, after the pre-switch `Konec` check:

```cpp
        bool sledovatZmenu =
            prikaz.typ != TypPrikazu::Zpet && prikaz.typ != TypPrikazu::Ulozit &&
            prikaz.typ != TypPrikazu::Napoveda && prikaz.typ != TypPrikazu::ZmenaHesla &&
            prikaz.typ != TypPrikazu::Neznamy;
        StavSeznamu zaloha;
        if (sledovatZmenu) zaloha = stav;
```

New case:

```cpp
            case TypPrikazu::Zpet:
                if (predchozi) {
                    std::swap(stav, *predchozi);
                    zprava = "Posledni zmena vracena. (u = znovu)";
                } else {
                    zprava = "Neni co vratit.";
                }
                break;
```

After the switch (before the post-switch `Konec` check):

```cpp
        if (sledovatZmenu && serializujSeznamy(zaloha) != serializujSeznamy(stav)) {
            predchozi = std::move(zaloha);
        }
```

- [ ] **Step 2:** `make all test` — PASS (unit suite unaffected).
- [ ] **Step 3:** E2E (scratchpad e2e dir, password `heslo`): `printf 'heslo\np pokus\nu\nu\nq\n'` — after first `u` the task disappears ("Posledni zmena vracena."), after second `u` it's back (redo). Also `d` then `u` restores the deleted list.
- [ ] **Step 4:** Commit: `git commit -am "feat: u command undoes the last change"`

---

### Task 8: `zh` — change password (main only)

**Files:** Modify `todolist.cpp`

- [ ] **Step 1:** New case (reuses existing `nastavNovyKlic`):

```cpp
            case TypPrikazu::ZmenaHesla: {
                std::vector<unsigned char> novyKlic;
                std::array<unsigned char, crypto_pwhash_SALTBYTES> novaSul{};
                if (nastavNovyKlic(novyKlic, novaSul)) {
                    klic = std::move(novyKlic);
                    sul = novaSul;
                    zprava = ulozSeznamy(stav, soubor, klic, sul)
                                 ? "Heslo zmeneno a ulozeno."
                                 : "CHYBA: Ulozeni se nezdarilo, zkus to znovu.";
                } else {
                    zprava = "Zmena hesla zrusena.";
                }
                break;
            }
```

- [ ] **Step 2:** `make all test` — PASS.
- [ ] **Step 3:** E2E: `printf 'stare\nstare\nzh\nnove\nnove\nq\n'` on a fresh file, then reopen with `printf 'nove\nq\n'` — decrypts with the new password.
- [ ] **Step 4:** Commit: `git commit -am "feat: zh command changes the password"`

---

### Task 9: Footer + final help page

**Files:** Modify `todolist.hpp` (`vykresliObrazovku` bottom, `vytiskniNapovedu`), Test `test_todolist.cpp` (help test + 3 screen tests)

- [ ] **Step 1:** Update expected strings first. In the three screen tests replace

```
"Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q | h\n"
```

with

```
"\033[90mukol: p pridat · o hotovo · r odebrat · e upravit · m presunout · c uklidit\n"
"seznam: n novy · v vybrat · j prejmenovat · d smazat | u zpet · s ulozit · q konec · h napoveda\033[0m\n"
```

In `test_vytiskni_napovedu` add after the `r <id>` line:

```
"  e <id> <popis>   Upravi popis ukolu.\n"
"  m <id> <sid>     Presune ukol do seznamu <sid>.\n"
"  c                Odstrani hotove ukoly z aktivniho seznamu.\n"
```

and in OSTATNI before the `s` line:

```
"  u                Vrati posledni zmenu (u znovu = zpet).\n"
```

and after the `q` line:

```
"  zh               Zmeni heslo souboru.\n"
```

- [ ] **Step 2:** `make test` — expected: assertion FAIL
- [ ] **Step 3:** Apply the same changes to `vytiskniNapovedu` and to the bottom of `vykresliObrazovku`:

```cpp
    out << "\033[90mukol: p pridat · o hotovo · r odebrat · e upravit · m presunout · c uklidit\n"
           "seznam: n novy · v vybrat · j prejmenovat · d smazat | u zpet · s ulozit · q konec · h napoveda\033[0m\n"
        << "> ";
```

- [ ] **Step 4:** `make all test` — PASS. Commit: `git commit -am "feat: labeled dim footer and complete help page"`

---

### Task 10: List-bar wrapping

**Files:** Modify `todolist.hpp` (`vytiskniSeznamy`, `vykresliObrazovku` gains `int sirka = 80`), `todolist.cpp` (width detection), Test `test_todolist.cpp`

- [ ] **Step 1: Failing tests:**

```cpp
void test_vytiskni_seznamy_zalamovani() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Alfa", {}}, {2, "Beta", {}}, {3, "Gama", {}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vytiskniSeznamy(out, stav, 45);
    // 9 + 17 (aktivni) + 3 + 15 = 44 <= 45; Gama by presahla -> zalom
    assert(out.str() ==
        "Seznamy: \033[1m>[1] Alfa (0.0%)<\033[0m | [2] Beta (0.0%)\n"
        "         [3] Gama (0.0%)\n");
}

void test_vytiskni_seznamy_uzka_obrazovka() {
    StavSeznamu stav;
    stav.seznamy = {{1, "Alfa", {}}, {2, "Beta", {}}};
    stav.aktivniId = 1;
    std::ostringstream out;
    vytiskniSeznamy(out, stav, 10);
    // kazda polozka sirsi nez sirka -> jedna na radek
    assert(out.str() ==
        "Seznamy: \033[1m>[1] Alfa (0.0%)<\033[0m\n"
        "         [2] Beta (0.0%)\n");
}
```

- [ ] **Step 2:** `make test` — compile FAIL (no 3-arg overload)
- [ ] **Step 3:** Replace `vytiskniSeznamy`:

```cpp
// Vypíše řádek seznamů; při přesahu šířky zalamuje s odsazením pod
// "Seznamy: ". Viditelná délka se počítá bez ANSI kódů (bajty, ASCII).
inline void vytiskniSeznamy(std::ostream& out, const StavSeznamu& stav, int sirka = 80) {
    out << "Seznamy: ";
    int delkaRadku = 9;
    bool prvniNaRadku = true;
    for (const auto& seznam : stav.seznamy) {
        std::string polozka = "[" + std::to_string(seznam.id) + "] " + seznam.nazev
                              + " (" + formatujProcenta(seznam.ukoly) + ")";
        bool aktivni = (seznam.id == stav.aktivniId);
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

`vykresliObrazovku` signature gains `int sirka = 80` (last parameter), first line becomes `vytiskniSeznamy(out, stav, sirka);`. In `todolist.cpp` add `#include <sys/ioctl.h>` and:

```cpp
// Šířka terminálu; mimo terminál (pipe) nebo při chybě 80.
int sirkaTerminalu() {
    winsize ws{};
    if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return 80;
}
```

and the render call becomes `vykresliObrazovku(std::cout, stav, zprava, sirkaTerminalu());`.

- [ ] **Step 4:** `make all test` — PASS. Commit: `git commit -am "feat: wrap list bar to terminal width"`

---

### Task 11: Fixed data-file location + migration

**Files:** Modify `todolist.cpp` (includes, `cestaKSouboru`, `main` load)

- [ ] **Step 1:** Add `#include <filesystem>` and `#include <cstdlib>`, plus:

```cpp
// Datový soubor v $XDG_DATA_HOME/todolist/, jinak ~/.local/share/todolist/.
// Bez obou proměnných (nebo při chybě) zůstává ukoly.txt v aktuálním adresáři.
std::string cestaKSouboru() {
    std::filesystem::path zaklad;
    const char* xdg = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    if (xdg && *xdg) {
        zaklad = xdg;
    } else if (home && *home) {
        zaklad = std::filesystem::path(home) / ".local" / "share";
    } else {
        return "ukoly.txt";
    }
    std::filesystem::path adresar = zaklad / "todolist";
    std::error_code ec;
    std::filesystem::create_directories(adresar, ec);
    if (ec) return "ukoly.txt";
    return (adresar / "ukoly.txt").string();
}
```

In `main()`: `const std::string soubor = cestaKSouboru();` and after the first `nactiObsahSouboru(soubor)`:

```cpp
    if (!obsah && soubor != "ukoly.txt") {
        obsah = nactiObsahSouboru("ukoly.txt");
        if (obsah) {
            std::cout << "Soubor ukoly.txt z aktualniho adresare byl nacten; nove se uklada do "
                      << soubor << ".\n";
        }
    }
```

(The existing `if (!obsah) ... else if (jeSifrovany(*obsah)) ... else ...` chain then works unchanged; saving always targets `soubor`.)

- [ ] **Step 2:** `make all test` — PASS.
- [ ] **Step 3:** E2E with isolated `XDG_DATA_HOME` (never the real one):

```bash
cd <scratchpad>/e2e && export XDG_DATA_HOME=$PWD/xdg && rm -rf xdg
printf '1;stary ukol;0\n' > ukoly.txt
printf 'heslo\nheslo\nq\n' | /home/mgardea/todolist/todolist.exe   # migruje z cwd
ls xdg/todolist/ukoly.txt                                          # existuje
cd / && XDG_DATA_HOME=<scratchpad>/e2e/xdg printf... 'heslo\nq\n' | todolist.exe  # najde data i odjinud
```

- [ ] **Step 4:** Commit: `git commit -am "feat: store data file under XDG data dir with cwd migration"`

---

### Task 12: Full e2e sweep, review, merge

- [ ] Run the full unit suite (`make test`) and an e2e pass over: undo/redo, zh + reopen, wrapped bar (many lists via pipe — width falls back to 80, so create enough lists to exceed 80 cols), migration, truthful final save message.
- [ ] superpowers:requesting-code-review over the branch; fix findings.
- [ ] superpowers:finishing-a-development-branch (user preference: merge to master locally).

## Verification summary

- `make test` → `Vsechny testy prosly.`
- E2E scripted runs per task above, all with `XDG_DATA_HOME` pointed into the scratchpad after Task 11.
