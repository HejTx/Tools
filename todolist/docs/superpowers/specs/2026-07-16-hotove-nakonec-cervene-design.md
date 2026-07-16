# Design: Hotové úkoly nakonec + prošlé termíny červeně

Úkoly #9 a #13 ze sdíleného seznamu todoDev; přístup schválen uživatelem
v konverzaci.

## 1. Hotové nakonec při řazení podle termínu (#9)
Komparátor režimu 2 (termín) dostane primární klíč `done` (nehotové
před hotovými); dosavadní klíče následují: `(done, klicTerminu, id)`
v `serazeneUkoly` a `(done, klicTerminu, seznamId, id)` v `sestavPrehled`.
Režim 1 (podle ID) se nemění.

## 2. Prošlé termíny červeně (#13)
- Prošlý = nehotový úkol s termínem ostře před dneškem (`klicTerminu <
  dnešní klíč`); úkol s termínem dnes prošlý není. Hotové úkoly zůstávají
  celé šedé (šedá má přednost).
- Prošlý řádek se obalí červeně `\033[31m ... \033[0m` (celý řádek,
  stejný vzor jako šedá u hotových).
- Dnešek: nový helper v `todolist.cpp` (systémový čas, netestuje se):
  ```cpp
  // Dnešní datum jako klíč rr*10000+mm*100+dd (formát klicTerminu).
  int dnesniKlic();  // std::time + localtime_r
  ```
- Render funkce jsou čisté a testovatelné — dnešek se injektuje:
  `vytiskniUkol(out, ukol, prefixId = "", dnes = 0)`,
  `vytiskniUkoly(out, ukoly, dnes = 0)` a `vykresliObrazovku(..., int
  sirka = 80, int dnes = 0)`. `dnes = 0` = kontrola vypnutá (žádný klíč
  není < 0), stávající testy beze změny. `main()` předává
  `dnesniKlic()`.

## Testy
- `serazeneUkoly`/`sestavPrehled`: hotový úkol s nejbližším termínem
  končí za nehotovými; mezi hotovými pořadí podle termínu.
- `vytiskniUkol`: prošlý → `\033[31m...\033[0m`; termín dnes → bez
  barvy; prošlý hotový → šedý; `dnes = 0` → bez barvy.
- Exact-string test obrazovky s prošlým úkolem.
- E2E: seznam s prošlým, dnešním a hotovým-prošlým úkolem.

## Mimo rozsah
- Konfigurace barev; jiná zvýraznění (blížící se termín apod.).
- Překreslení při změně data za běhu (klíč se čte při každém redraw,
  takže půlnoc se projeví při další akci).
