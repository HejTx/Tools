# Design: Příkaz `h` — nápověda (man page)

## Cíl
Nový příkaz `h` zobrazí místo seznamu úkolů manuálovou stránku všech
příkazů. Modální chování: stránka čeká na Enter a pak se vrátí na běžnou
obrazovku. Z nápovědy nelze rovnou spouštět příkazy.

## Chování
- `rozeberPrikaz("h")` → nový typ `TypPrikazu::Napoveda` (bez argumentů;
  případný zbytek řádku se ignoruje stejně jako u `s`/`q`).
- `main()` ve větvi `Napoveda`: smaže obrazovku (`\033[2J\033[H`), vytiskne
  stránku, počká na jeden `getline`. Vstup se zahodí — Enter vrátí zpět;
  EOF ukončí smyčku (uloží a skončí, jako všude jinde). Žádný nový stav;
  další iterace vykreslí běžnou obrazovku bez stavové zprávy.

## Vykreslení
Nová čistá funkce v `todolist.hpp` (testovatelná exact-string testem):

```cpp
inline void vytiskniNapovedu(std::ostream& out);
```

Přesný obsah (ASCII, bez diakritiky jako zbytek UI):

```
TODOLIST(1)                        Napoveda

PRIKAZY UKOLU
  p <popis>        Prida novy ukol do aktivniho seznamu.
  o <id>           Oznaci ukol jako hotovy.
  r <id>           Odebere ukol ze seznamu.

PRIKAZY SEZNAMU
  n <nazev>        Zalozi novy seznam a prepne na nej.
  v <id>           Prepne na seznam podle ID.
  j <id> <nazev>   Prejmenuje seznam.
  d                Smaze aktivni seznam (bez potvrzeni!).
  d <id>           Smaze seznam podle ID.

OSTATNI
  s                Ulozi vsechny seznamy.
  q                Ulozi a ukonci program.
  h                Zobrazi tuto napovedu.

Pokracuj stiskem Enteru...
```

Výstup končí `\n` za posledním řádkem; kurzor stojí na novém řádku a
`getline` čeká na Enter.

## Objevitelnost
Spodní řádek hlavní obrazovky dostane ` | h` na konec:

```
Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q | h
```

## Testy
- Parser: `rozeberPrikaz("h")` → `Napoveda`.
- Render: exact-string test `vytiskniNapovedu`.
- Aktualizace tří exact-string testů obrazovky (nový spodní řádek s `| h`).
- E2E: piped běh `h` → Enter → `q` (nápověda se zobrazí, návrat funguje).

## Mimo rozsah
- Stránkování / barvy v nápovědě.
- Nápověda k jednotlivým příkazům (`h p`).
