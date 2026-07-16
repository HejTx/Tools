# Design: Balík vylepšení (polish batch)

Devět menších funkcí schválených najednou ("do everything"). Uživatelská
rozhodnutí: **u vrací jakoukoli poslední změnu** (ne jen smazání seznamu)
a spodní nápovědní řádek nahrazuje **popsaný šedý footer** (volba
"Labeled dim footer").

## 1. Makefile
Cíle: `all` (todolist.exe), `test` (přeloží a spustí testy), `clean`.
Kompilace beze změny: `g++ ... -lsodium`. `.PHONY: all test clean`.

## 2. Pravdivé hlášky o uložení
`ulozSeznamy` vrací `bool` (false = nešlo otevřít, zápis selhal, nebo
selhal rename). `main()`:
- příkaz `s`: úspěch → "Ukoly ulozeny.", selhání → "CHYBA: Ulozeni se
  nezdarilo, zkus to znovu."
- závěrečné uložení: úspěch → "Ukoly ulozeny. Nashledanou." (jako dnes),
  selhání → "POZOR: Ukoly se nepodarilo ulozit!" a `return 1`.
- `zh` (bod 8) hlásí uložení stejně pravdivě.

## 3. Footer místo řádku "Prikazy:"
Dva šedé řádky (`\033[90m`, jako hotové úkoly) s českými slovesy;
UTF-8 tečka `·` jako oddělovač:

```
ukol: p pridat · o hotovo · r odebrat · e upravit · m presunout · c uklidit
seznam: n novy · v vybrat · j prejmenovat · d smazat | u zpet · s ulozit · q konec · h napoveda
```

Pořadí obrazovky: řádek seznamů, titulek, úkoly, prázdný řádek,
[zpráva + prázdný řádek], footer (2 řádky), `> `. Detaily příkazů žijí
v `h`.

## 4. `e <id> <popis>` — úprava úkolu
`upravitUkol(ukoly, id, popis)` → bool; mění jen popis, ID a done
zůstávají. Validace v main: prázdný popis → "Popis nesmi byt prazdny.",
`;` → stávající hláška, nenalezen → "Ukol s ID x nenalezen.",
úspěch → "Ukol upraven."

## 5. `m <id> <sid>` — přesun úkolu
`presunUkol(stav, ukolId, cilId)` → int: 0 = OK, 1 = úkol není
v aktivním seznamu, 2 = cílový seznam neexistuje, 3 = cíl je aktivní
seznam. Úkol dostane v cílovém seznamu nové ID (per-seznam číslování),
popis a done se zachovají. Hlášky: "Ukol presunut do seznamu '<nazev>'.",
"Ukol s ID x nenalezen.", "Seznam s ID x nenalezen.",
"Ukol uz je v aktivnim seznamu."
Parser: `Prikaz` dostane pole `int id2 = -1`; helper `nactiIdArgument`
dostane parametr `int& cil`, volající předávají `prikaz.id` nebo
`prikaz.id2` (stávající volání se upraví).

## 6. `c` — úklid hotových
`vycistiHotove(ukoly)` → int (počet odstraněných). Hlášky bez českých
plurálů: 0 → "Zadne hotove ukoly k odstraneni.", jinak
"Odstraneno hotovych ukolu: X."

## 7. `u` — vrácení poslední změny
Jednoúrovňové undo čehokoli. V `main()` `std::optional<StavSeznamu>
predchozi`. Mechanismus bez příznaků po větvích: před switchem (kromě
`u`, `s`, `h`, `zh`, `q`, Neznamy) se vezme kopie `zaloha = stav`;
po switchi když `serializujSeznamy(zaloha) != serializujSeznamy(stav)`,
uloží se `predchozi = zaloha`. Příkaz `u`: když `predchozi` existuje,
`std::swap(stav, *predchozi)` → "Posledni zmena vracena. (u = znovu)";
swap přirozeně dává redo. Jinak "Neni co vratit."
Pozn.: i přepnutí `v` je vratná změna (mění `@aktivni`).

## 8. `zh` — změna hesla
Znovu použije existující `nastavNovyKlic` (2× skrytý dotaz, nová sůl,
odvození klíče) a hned uloží: úspěch → "Heslo zmeneno a ulozeno.",
selhání uložení → hláška z bodu 2, EOF při zadávání → "Zmena hesla
zrusena." (starý klíč zůstává).

## 9. Zalamování řádku seznamů
`vytiskniSeznamy(out, stav, sirka)` — nový parametr šířky. Položky se
skládají s oddělovačem ` | `; když by další položka přesáhla `sirka`
(počítá se viditelná délka bez ANSI kódů; aktivní položka +2 za `>`/`<`),
odřádkuje se a pokračuje s odsazením 9 mezer (pod "Seznamy: ").
Na řádku je vždy aspoň jedna položka. `main()` zjistí šířku terminálu
přes `ioctl(STDOUT_FILENO, TIOCGWINSZ)`; když stdout není terminál nebo
ioctl selže, použije se 80. Testy volají funkci s explicitní šířkou.

## 10. Pevné umístění souboru
`cestaKSouboru()` v `todolist.cpp`: `$XDG_DATA_HOME/todolist/ukoly.txt`,
jinak `$HOME/.local/share/todolist/ukoly.txt`; adresáře se založí
(`std::filesystem::create_directories`). Bez `$HOME` i `$XDG_DATA_HOME`
fallback na dnešní `ukoly.txt` v cwd.
Migrace: když nová cesta neexistuje a v cwd leží `ukoly.txt`, načte se
obsah z cwd (šifrovaný i starý formát) a oznámí se
"Soubor ukoly.txt nalezen v aktualnim adresari, data se presouvaji do
<nova cesta>." Ukládá se vždy na novou cestu; starý soubor se nechává
na místě.
E2E testy nastavují `XDG_DATA_HOME` do scratchpadu, aby se nesahalo na
skutečná data.

## Nápověda (`h`) — finální podoba
Sekce PRIKAZY UKOLU doplněná o `e`, `m <id> <sid>`, `c`; sekce OSTATNI
o `u` a `zh` (před `h`). Přesné znění řádků:

```
  e <id> <popis>   Upravi popis ukolu.
  m <id> <sid>     Presune ukol do seznamu <sid>.
  c                Odstrani hotove ukoly z aktivniho seznamu.
  u                Vrati posledni zmenu (u znovu = zpet).
  zh               Zmeni heslo souboru.
```

## Parser — nové typy
`UpravitUkol` (`e`), `PresunoutUkol` (`m`), `VycistitHotove` (`c`),
`Zpet` (`u`), `ZmenaHesla` (`zh`). `e` parsuje id + zbytek řádku (jako
`j`); `m` parsuje dvě ID; `c`/`u`/`zh` jsou bez argumentů (zbytek řádku
se ignoruje jako u `s`/`q`/`h`).

## Testy
Ke každému bodu testy ve stávajícím stylu: parser (`e 2 novy text`,
`m 3 2`, chybné varianty…), `upravitUkol`, `presunUkol` (všechny čtyři
výsledky), `vycistiHotove` (0 i více), zalamování `vytiskniSeznamy`
(vejde se / zalomí / jedna položka širší než šířka), aktualizované
exact-string testy obrazovky (footer) a nápovědy, `ulozSeznamy` vrací
true při úspěchu (test zápisu do scratchpad souboru + round-trip přes
`desifruj`). Undo a `zh` se ověří e2e (žijí v `main()`).

## Mimo rozsah
- Víceúrovňové undo / historie.
- Detekce změny šířky terminálu za běhu (SIGWINCH).
- Konfigurovatelná cesta k souboru (env/flag).
- Přesun úkolu z jiného než aktivního seznamu.
