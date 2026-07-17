# Design: Git synchronizace a historie verzí (todoDev #4)

Rozhodnutí uživatele: sdílené úložiště přes git (soukromý GitHub repo),
zařízení WSL + libovolný Linux; plná historie verzí s obnovou.

## Aktivace — bez konfigurace
Synchronizace je zapnutá právě tehdy, když je datový adresář git
repozitář (existuje `.git`). Jinak se aplikace chová jako dosud.
Zřízení = `git clone <repo> ~/.local/share/todolist` (nové zařízení),
nebo `git init` + `remote add` v existujícím adresáři. Při startu
v repozitáři aplikace zajistí `.gitignore` s obsahem:
```
nastaveni.txt
*.tmp
*.poskozeny
```
(předvolby a odpad zůstávají lokální; synchronizují se jen trezory
a archivy).

## Průběh
- **Start:** `git -C <dir> pull --ff-only --quiet`. Neúspěch (offline
  nebo divergentní historie) → jednořádkové varování
  „Synchronizace: pull se nezdaril (offline, nebo mistni a vzdalene
  zmeny koliduji)." a běží se dál nad lokálním stavem. Divergence se
  neřeší automaticky (binární šifrované soubory nejde slévat); soubory
  po seznamech ji činí vzácnou.
- **Uložení (`s`/`q`):** po zápisu souborů `git add -A`, `git commit
  -m "ulozeni" --quiet` (nic ke commitu = v pořádku) a `git push
  --quiet`. Selhání pushe nekazí uložení: hláška „Ukoly ulozeny mistne,
  synchronizace se nezdarila (offline?)." (u `q` obdobně v závěrečné
  hlášce); commit zůstává a odejde při příštím uložení. Úspěch u `s` →
  „Ukoly ulozeny a synchronizovany." (bez repa zůstává „Ukoly
  ulozeny.").
- `zh` ukládá soubory přímo (mimo `ulozVse`) — commitne se při
  nejbližším `s`/`q`.

## Historie verzí — příkaz `vz`
- `vz` (bez argumentu): modální stránka (vzor `h`) s očíslovanými
  verzemi trezoru aktivního seznamu — číslo, datum a čas commitu,
  nejnovější první (z `git log` nad souborem seznamu).
- `vz <n>`: obnoví verzi č. n. Postup: commit aktuálního stavu
  („pred obnovou", i obnova je tedy jen další verzí), `git checkout
  <hash> -- <soubor seznamu>`; archivní soubor se obnoví, pokud v tom
  commitu existoval (`git ls-tree`), jinak se lokální archiv smaže
  (stav odpovídá snímku). Seznam se v paměti **zamkne** (pahýl) a hláška
  vyzve k odemčení přes `v <id>` — heslem platným v době snímku (řeší
  obnovu před `zh` bez žonglování s hesly).
- `vz` mimo repo → „Synchronizace neni zapnuta (datovy adresar neni
  git repozitar)."; v přehledu 0 → „Prepni na konkretni seznam.";
  neplatné číslo → „Verze <n> nenalezena."
- Obnova není vratná přes `u` (typ `Verze` se nesnapshotuje — vratnost
  zajišťuje sama historie: `vz 1` je stav před obnovou).

## Implementace
- `todolist.cpp`: `jeGitRepo(adresar)`, `gitPrikaz(adresar, argumenty)`
  → celý příkazový řetězec (adresář v uvozovkách; čistá funkce,
  testovatelná v hpp? → builder `sestavGitPrikaz` jde do hpp kvůli
  testům), spouštění přes `std::system` (tiché varianty, kontrola
  návratových kódů), výpis logu přes `popen`.
- `todolist.hpp` (čisté, testované): `sestavGitPrikaz(adresar, args)`,
  `parsujVerze(vystupLogu)` → `std::vector<VerzeZaznam { std::string
  hash; std::string datum; }>` (formát řádku `hash;datum`),
  `vytiskniVerze(out, nazevSeznamu, verze)` — modální stránka.
- Log formát: `git log --format=%h;%ad --date=format:"%d/%m/%y %H:%M"
  -- <soubor>`.
- Parser: `vz` → `TypPrikazu::Verze` (bez čísla id = -1 → výpis);
  `vz <n>` → id = n (n < 1 → `Neznamy`, jako u `d`).
- Footer, řádek „jine": doplnit ` · vz verze` (68+11 = 79 ≤ 80 ✓).
- Nápověda, sekce OSTATNI (za `z`):
  `  vz [n]           Verze aktivniho seznamu; vz <n> obnovi verzi n.`
- README: sekce „Synchronizace" (zřízení, soukromý repo — názvy
  seznamů jsou vidět jako názvy souborů, obsah je šifrovaný; `vz`).

## Testy
- `sestavGitPrikaz` (uvozovky, tvar), `parsujVerze` (víc řádků, prázdný
  vstup, poškozený řádek se přeskočí), `vytiskniVerze` exact-string
  (s verzemi i prázdné), parser `vz`/`vz 3`/`vz 0`/`vz x`.
- E2E proti lokálnímu bare repu (`file://`, bez sítě): uložení →
  commit+push dorazí do bare; „druhé zařízení" = klon bare → vidí
  seznam; dvě verze → `vz` je vypíše, `vz 2` obnoví starší (seznam
  zamčený, po odemčení starý obsah); divergence → varování při startu;
  bez repa vše beze změny.

## Mimo rozsah
- Automatické řešení divergencí / slévání.
- Synchronizace uprostřed sezení (jen start + uložení).
- `vz` pro smazané seznamy (ručně přes git).
- Zřizování vzdáleného repa aplikací.
