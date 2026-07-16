# Design: Termíny, řazení a přehled „Vše" (v 0)

Tři propojené funkce schválené uživatelem: termín dokončení u úkolu,
cyklické přepínání řazení a virtuální seznam 0 se všemi úkoly.
Rozhodnutí uživatele: termín se nastavuje příkazem `t`; řazení přepíná
`z`; v přehledu 0 fungují jen příkazy nezávislé na holém ID úkolu —
úkolové příkazy tam vyžadují složené ID `<seznam>.<ukol>`.

## 1. Termín dokončení
- `Task` dostane `std::string termin;` (prázdný = bez termínu).
- Formát `dd/mm/yy`. `jePlatnyTermin()`: přesně 8 znaků, číslice
  a lomítka na pozicích 2 a 5, den 01–31, měsíc 01–12 (bez kontroly
  délky měsíce a přestupných let — mimo rozsah).
- `klicTerminu()`: rr*10000 + mm*100 + dd pro porovnávání; prázdný
  termín = INT_MAX (řadí se nakonec).
- Příkaz `t <id> <datum>` termín nastaví („Termin nastaven na X."),
  `t <id>` bez data smaže („Termin odstranen."), špatný formát →
  „Neplatny format terminu, pouzij dd/mm/yy.", neexistující úkol →
  stávající hláška. Helper `nastavTermin(ukoly, id, termin)` → bool.
- Serializace: `id;popis;done;termin` (4. pole vždy, i prázdné).
  `parsujUkoly` čte 4. pole volitelně — staré 3-polové řádky dají
  prázdný termín (zpětná kompatibilita).
- Zobrazení: `, Termin: 18/07/26` na konci řádku úkolu, jen když je
  termín nastaven (hotové úkoly zůstávají celé šedě).

## 2. Řazení
- `StavSeznamu` dostane `int razeni = 1;` (1 = podle ID, 2 = podle
  termínu). Persistuje se řádkem `@razeni;<n>` hned za `@aktivni;<id>`;
  chybějící nebo neplatná hodnota → 1.
- Příkaz `z` cykluje režim a hlásí „Razeni: podle terminu." /
  „Razeni: podle ID." Řazení je jen zobrazovací — pořadí v datech ani
  ID se nemění. Přepnutí je vratné přes `u` (mění serializaci).
- `serazeneUkoly(ukoly, razeni)` → kopie: režim 1 = pořadí v datech,
  režim 2 = stabilně podle (klicTerminu, id) — s termínem dřív,
  nejbližší první, bez termínu nakonec podle ID.
- Aktuální režim je vidět v titulku: `=== Nakup === (razeni: ID)`.

## 3. Složená ID `<seznam>.<ukol>`
- `Prikaz` dostane `int seznamUkolu = -1;`. Parserový helper
  `nactiUkolId()`: token bez tečky → jen `id`; s tečkou → obě části
  `stoi` (`2.`, `.3`, `2.x` → `Neznamy`).
- Přijímají ho `o`, `r`, `e`, `t` a první argument `m`. Funguje
  odkudkoli: `o 2.3` splní úkol 3 v seznamu 2 i mimo přehled.
- `presunUkol` dostane parametr zdrojového seznamu:
  `presunUkol(stav, zdrojId, ukolId, cilId)` → 0 OK, 1 úkol nenalezen,
  2 cílový seznam neexistuje, 3 cíl == zdroj (4 = zdroj neexistuje se
  neřeší — zdroj resolvuje main a hlásí „Seznam ... nenalezen." dřív).
- Rozhodnutí cílového seznamu v `main()` (sdílená logika pro všechny
  úkolové příkazy): složené ID → daný seznam (neexistuje → hláška);
  holé ID v přehledu 0 → „V prehledu pouzij ID ve tvaru
  <seznam>.<ukol>, napr. o 2.3."; jinak aktivní seznam.

## 4. Přehled „Vše" (seznam 0)
- `v 0` přepne na virtuální přehled; `aktivniId == 0` je platný stav
  (`vybratSeznam` ho povolí, `parsujSeznamy` normalizace i
  `smazatSeznam` ho zachovávají — po smazání seznamu v přehledu
  zůstává aktivní 0; smazání posledního seznamu založí `Ukoly`,
  aktivní zůstává 0).
- Řádek seznamů začíná vždy pseudo-položkou `[0] Vse (x%)` s celkovým
  procentem (helper `formatujProcenta(hotovo, celkem)`, stávající
  vektorová varianta ho volá). Zvýrazněná, když je přehled aktivní;
  zalamuje se jako ostatní položky.
- Tělo přehledu: `sestavPrehled(stav)` → vektor
  `PolozkaPrehledu { int seznamId; Task ukol; }`. Režim 1: pořadí
  seznam 1, seznam 2, … (v každém pořadí dat). Režim 2: stabilně podle
  (klicTerminu, seznamId, id). Řádky úkolů mají složené ID:
  `ID: 2.1, Popis: ...` (prefix parametr `vytiskniUkol`).
  Titulek `=== Vse === (razeni: ...)`; prázdné → „Zadne ukoly."
- Příkazy v přehledu: `o/r/e/t/m` jen se složeným ID; `p` →
  „V prehledu nelze pridavat, prepni na seznam."; `d` bez ID →
  „V prehledu neni aktivni seznam ke smazani."; `c` → úklid hotových
  napříč všemi seznamy (jedna hláška s počtem, vratné přes `u`);
  `n/v/j/d <id>/u/z/s/q/zh/h` beze změny. `v 0` hlásí „Prepnuto na
  prehled 'Vse'."

## 5. Footer a nápověda
Footer (3 šedé řádky, ≤ 80 sloupců):
```
ukol: p pridat · o hotovo · r odebrat · e upravit · m presunout · t termin
seznam: n novy · v vybrat · j prejmenovat · d smazat · c uklidit
jine: u zpet · z razeni · s ulozit · zh heslo · q konec · h napoveda
```
Nápověda (`h`) — nové/změněné řádky:
```
  t <id> <datum>   Nastavi termin (dd/mm/yy); t <id> bez data termin smaze.
  c                Odstrani hotove ukoly (v prehledu 0 ze vsech seznamu).
  v <id>           Prepne na seznam podle ID; v 0 = prehled vsech ukolu.
  z                Prepina razeni: podle ID / podle terminu.
```
a před „Pokracuj stiskem Enteru..." poznámka:
```
Ukoly lze adresovat i jako <seznam>.<ukol> (napr. o 2.3) - v prehledu 0 je to nutne.
```

## Testy
- `jePlatnyTermin` (platné, špatné délky/oddělovače/rozsahy),
  `klicTerminu` (pořadí, prázdný = nakonec).
- Serializace se 4. polem + round-trip; parsování starého 3-polového
  řádku; `@razeni` round-trip a default.
- Parser: `o 2.3`, `t 2 18/07/26`, `t 2.3 18/07/26`, `t 2` (mazání),
  `z`, `m 2.3 4`, chybné `o 2.`, `o .3`, `o 2.x`.
- `nastavTermin`, `serazeneUkoly` (oba režimy, remízy, bez termínu),
  `sestavPrehled` (oba režimy), `presunUkol` se zdrojem,
  `vybratSeznam(0)`, `smazatSeznam` při aktivním 0.
- Render: `[0] Vse` v baru (i zalamování — šířky přepočítat), titulek
  s režimem, termín v řádku úkolu, přehled v obou režimech, footer,
  nápověda — exact-string.
- E2E: nastavení/mazání termínu, `z` cyklus, `v 0` v obou režimech,
  `o 2.3` z přehledu i z běžného seznamu, `c` v přehledu, persistence
  `@razeni` a `@aktivni;0` přes restart.

## Mimo rozsah
- Zvýraznění prošlých termínů (vyžaduje dnešní datum).
- Plná kalendářní validace (délky měsíců, přestupné roky).
- Řazení podle dalších klíčů (abeceda, hotovost).
- Přesun/adresace seznamů složeným ID (`j 2.3` nedává smysl).
