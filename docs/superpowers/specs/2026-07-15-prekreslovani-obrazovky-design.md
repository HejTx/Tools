# Design: Překreslování obrazovky (mini-TUI)

## Cíl
Výstup příkazu dnes okamžitě odsune 8řádková nápověda vypsaná před dalším vstupem.
Nově se po každém příkazu obrazovka smaže a překreslí: seznam úkolů nahoře, zpráva
o výsledku poslední akce, kompaktní nápověda a prompt dole. Vše čistě v terminálu
pomocí ANSI escape sekvencí, bez externích závislostí.

## Tok programu
1. `main()` načte úkoly z `ukoly.txt`.
2. Smyčka: vypíše `"\033[2J\033[H"` (smazání obrazovky + kurzor domů), zavolá
   `vykresliObrazovku`, přečte řádek (`std::getline`), rozparsuje (`rozeberPrikaz`),
   provede akci a výsledek uloží do proměnné `zprava` (`std::string`), která se
   zobrazí při dalším překreslení.
3. Při `q` nebo EOF se úkoly uloží a program skončí rozlučkovou hláškou — bez
   smazání obrazovky, aby poslední stav zůstal vidět.

## Render funkce
Do `todolist.hpp` přibude:

```cpp
void vykresliObrazovku(std::ostream& out,
                       const std::vector<Task>& ukoly,
                       const std::string& zprava);
```

Vypíše:

```
=== Ukoly ===
ID: 1, Popis: nakoupit, Dokonceno: Ano      <- šedě, je-li hotový
ID: 2, Popis: uklidit, Dokonceno: Ne

Ukol 1 oznacen jako hotovy.

Prikazy: p <popis> | o <id> | r <id> | s (ulozit) | q (ulozit a konec)
>
```

- Prázdný seznam → `Zadne ukoly.`
- Prázdná `zprava` → řádek se zprávou (i prázdný řádek kolem) se přeskočí.
- Hotové úkoly se vypisují šedě: řádek obalený `"\033[90m"` … `"\033[0m"`.
  Render tyto kódy vypisuje vždy; testy je zahrnou do očekávaného výstupu.
- Smazání obrazovky (`"\033[2J\033[H"`) vypisuje `main()`, ne render — render
  tak jde testovat čistě přes `std::ostringstream`.
- Stávající `vytiskniUkol`/`vytiskniUkoly` se upraví/použijí uvnitř render funkce
  (`vytiskniUkol` dostane `std::ostream&` a šedé obarvení hotových úkolů).

## Zprávy o výsledku
Každá akce nastaví `zprava`:

- `p` → `Ukol pridan.` / `Popis nesmi obsahovat znak ';'.`
- `o <id>` → `Ukol <id> oznacen jako hotovy.` / `Ukol s ID <id> nenalezen.`
- `r <id>` → `Ukol <id> odebran.` / `Ukol s ID <id> nenalezen.`
- `s` → `Ukoly ulozeny.`
- neznámý příkaz → `Neznamy prikaz.`

## Odebrání příkazu `v`
Seznam je po každém překreslení viditelný, takže `v` ztrácí smysl:

- `TypPrikazu::Vypsat` se odebere z enumu.
- Větev pro `"v"` v `rozeberPrikaz` se smaže (spadne do `Neznamy`).
- Case `Vypsat` v `main()` se smaže.
- `vypisNapovedu()` v `todolist.cpp` se smaže (nahrazuje ji render).

## Testy
V `test_todolist.cpp` (assert testy, stejný styl jako dosud):

- Parser: `"v"` nově vrací `Neznamy`; ostatní testy beze změny.
- `vykresliObrazovku` přes `std::ostringstream`:
  - prázdný seznam (`Zadne ukoly.`),
  - seznam s hotovým i nehotovým úkolem (šedý řádek s `\033[90m`/`\033[0m`),
  - s neprázdnou zprávou a s prázdnou zprávou.

## Mimo rozsah
- Detekce podpory ANSI / vypínání barev (cílí se na WSL, Linux a moderní
  Windows Terminal, kde ANSI funguje).
- Ovládání kurzoru, ncurses, průběžné ukládání — beze změny chování.
