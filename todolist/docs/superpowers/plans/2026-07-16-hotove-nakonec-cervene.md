# Hotové nakonec + prošlé červeně Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** In termin-sort mode completed tasks sink to the end (#9); undone tasks past their due date render red (#13).

**Architecture:** Add `done` as the primary comparator key in `serazeneUkoly`/`sestavPrehled` (mode 2 only). Overdue = `!done && klicTerminu(termin) < dnes`, with `dnes` injected into the pure render functions (default 0 = disabled) and produced by a new `dnesniKlic()` (localtime) in `main()`.

**Tech Stack:** C++17, libsodium, make. Spec: `docs/superpowers/specs/2026-07-16-hotove-nakonec-cervene-design.md`

---

### Task 1: Done tasks last in termin sort (#9)

**Files:** Modify `todolist.hpp` (both comparators), Test `test_todolist.cpp`

- [ ] **Step 1: Failing tests** (extend the two existing sort tests):

In `test_serazene_ukoly` add after the existing asserts:

```cpp
    // hotove nakonec, i kdyz maji nejblizsi termin
    std::vector<Task> sHotovym = {{1, "hotovy blizky", true, "01/01/26"},
                                  {2, "nehotovy pozdejsi", false, "20/08/26"},
                                  {3, "hotovy pozdni", true, "31/12/26"}};
    std::vector<Task> serazene = serazeneUkoly(sHotovym, 2);
    assert(serazene[0].id == 2);                       // nehotovy prvni
    assert(serazene[1].id == 1 && serazene[2].id == 3); // hotove podle terminu
```

In `test_sestav_prehled` add at the end (state has razeni already 2):

```cpp
    stav.seznamy[0].ukoly.push_back({3, "hotovy blizky", true, "01/01/26"});
    std::vector<PolozkaPrehledu> sHotovym = sestavPrehled(stav);
    assert(sHotovym.back().ukol.id == 3);              // hotovy az nakonec
```

- [ ] **Step 2:** `make test` — assertion FAIL
- [ ] **Step 3:** Both mode-2 comparators get the primary key. `serazeneUkoly`:

```cpp
            if (a.done != b.done) return !a.done;      // hotove nakonec
            if (klicTerminu(a.termin) != klicTerminu(b.termin)) {
                return klicTerminu(a.termin) < klicTerminu(b.termin);
            }
            return a.id < b.id;
```

`sestavPrehled` comparator likewise starts with `if (a.ukol.done != b.ukol.done) return !a.ukol.done;`.

- [ ] **Step 4:** `make test` — PASS. **Step 5:** `git commit -am "feat: completed tasks sort last in termin mode"`

---

### Task 2: Overdue tasks in red (#13)

**Files:** Modify `todolist.hpp` (`vytiskniUkol`, `vytiskniUkoly`, `vykresliObrazovku`), `todolist.cpp` (`dnesniKlic`, render call), Test `test_todolist.cpp`

- [ ] **Step 1: Failing tests:**

```cpp
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
```

- [ ] **Step 2:** `make test` — compile FAIL (no 4th parameter)
- [ ] **Step 3:** `vytiskniUkol` gains `int dnes = 0` (4th param):

```cpp
inline void vytiskniUkol(std::ostream& out, const Task& ukol,
                         const std::string& prefixId = "", int dnes = 0) {
    bool prosly = !ukol.done && !ukol.termin.empty()
                  && dnes != 0 && klicTerminu(ukol.termin) < dnes;
    if (ukol.done) out << "\033[90m";
    else if (prosly) out << "\033[31m";
    out << "ID: " << prefixId << ukol.id << ", Popis: " << ukol.description
        << ", Dokonceno: " << (ukol.done ? "Ano" : "Ne");
    if (!ukol.termin.empty()) out << ", Termin: " << ukol.termin;
    if (ukol.done || prosly) out << "\033[0m";
    out << "\n";
}
```

`vytiskniUkoly` gains `int dnes = 0` and forwards (`vytiskniUkol(out, ukol, "", dnes)`); `vykresliObrazovku` gains trailing `int dnes = 0`, forwards to the overview branch (`vytiskniUkol(out, polozka.ukol, std::to_string(polozka.seznamId) + ".", dnes)`) and the list branch (`vytiskniUkoly(out, ukoly, dnes)`). In `todolist.cpp` add `#include <ctime>` plus:

```cpp
// Dnešní datum jako klíč rr*10000+mm*100+dd (formát klicTerminu).
int dnesniKlic() {
    std::time_t ted = std::time(nullptr);
    std::tm rozpad{};
    localtime_r(&ted, &rozpad);
    return (rozpad.tm_year % 100) * 10000 + (rozpad.tm_mon + 1) * 100 + rozpad.tm_mday;
}
```

and the render call becomes `vykresliObrazovku(std::cout, stav, zprava, sirkaTerminalu(), dnesniKlic());`.

- [ ] **Step 4:** `make all test` — PASS. **Step 5:** `git commit -am "feat: overdue tasks render red"`

---

### Task 3: E2E, review, merge, mark todoDev

- [ ] E2E in scratchpad (isolated `XDG_DATA_HOME`): list with an overdue task (e.g. `t 1 01/01/20`), a future task, and a completed overdue task → red only on the undone overdue one; `z` termin mode puts the done task last.
- [ ] Code review (subagent if available, inline otherwise); fix findings.
- [ ] Merge to master (user preference), delete branch.
- [ ] Mark #9 and #13 done in the shared todoDev vault (`o 9`, `o 13`).

## Verification summary

`make test` → all pass; e2e above; today-key read at every redraw so midnight rollover applies on next action.
