# Design: Trezory — každý seznam vlastní heslo

Každý seznam se stává samostatným šifrovaným souborem s vlastním heslem.
Sdílení = předání hesla jednoho seznamu; ostatní zůstávají kryptograficky
nedostupné. Rozhodnutí uživatele: odemykání za běhu (unlock-as-you-go)
a migrace se stávajícím heslem pro všechny seznamy.

## Uložení na disku
- Adresář `~/.local/share/todolist/seznamy/` (respektive
  `$XDG_DATA_HOME/todolist/seznamy/`), jeden soubor na seznam:
  `<nazev>.txt`. Kontejner beze změny (`TODOENC1`, vlastní sůl, vlastní
  klíč odvozený z hesla seznamu, nová nonce při každém zápisu).
- Plaintext uvnitř = jen řádky úkolů (`id;popis;done;termin`). Hlavičky
  `#seznam`/`@aktivni`/`@razeni` zanikají — jméno nese soubor.
- Název seznamu: nesmí být prázdný, obsahovat `;` ani `/` a nesmí začínat
  tečkou (nový helper `zkontrolujNazevSeznamu` vrací chybovou hlášku nebo
  prázdný string; nahrazuje dosavadní `zkontrolujNazev`). Unikátnost
  (vůči odemčeným i zamčeným) hlídá `n`/`j`.
- Globální nešifrované předvolby v `~/.local/share/todolist/nastaveni.txt`
  (názvy seznamů nejsou tajné — jsou to názvy souborů):
  ```
  razeni;1
  posledni;Nakup
  ```
  Čisté funkce `serializujNastaveni`/`parsujNastaveni` (tolerantní,
  chybějící soubor → výchozí hodnoty).

## Datový model
```cpp
struct Seznam {
    int id;                    // ID v rámci sezení (abecední pořadí při startu)
    std::string nazev;
    std::vector<Task> ukoly;   // jen odemčené
    bool odemceno = false;
    std::vector<unsigned char> klic;                                  // jen odemčené
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul{};        // jen odemčené
};
```
`StavSeznamu` (seznamy, aktivniId, razeni) zůstává. Zamčené seznamy jsou
pahýly (jen id + nazev). Invariant: `aktivniId` je 0 (přehled), nebo ID
odemčeného seznamu.

`serializujSeznamy`/`parsujSeznamy` se starým víceseznamovým formátem
zůstávají jen pro: (a) migraci starého souboru, (b) detekci změn pro
undo — `serializujSeznamy` se rozšíří o příznak zamčení
(`#seznam;<id>;<nazev>;zamceno` u pahýlů), aby porovnání pokrylo celý
viditelný stav.

## Start aplikace
1. Založí/prohledá `seznamy/`, soubory `*.txt` → názvy (stem), abecedně,
   ID 1..n → pahýly.
2. Není-li žádný soubor a existuje starý `ukoly.txt` → **migrace** (níž).
3. Výzva `Seznam [<posledni>]: ` — Enter přijme předvyplněný název
   z `nastaveni.txt` (bez něj prostá výzva `Seznam: `). EOF → konec.
4. Název existuje → `Heslo: ` (skryté, opakuje se při neúspěchu jako
   dnes) → seznam se odemkne a je aktivní.
5. Název neexistuje → `Seznam '<X>' neexistuje. Zalozit? (a/n)` —
   `a` → volba hesla 2× (stávající `zvolNoveHeslo`) → nový prázdný
   odemčený aktivní seznam (soubor vznikne při uložení); `n` → zpět
   na krok 3. Neplatný název → hláška a zpět na krok 3.

Do hlavní smyčky se vstupuje vždy s ≥ 1 odemčeným seznamem.

## Sezení — odemykání za běhu
- Řádek seznamů: `[0] Vse` (procenta jen z odemčených) + odemčené
  s procenty + zamčené jako `[3] Tajne (zamceno)`.
- `v <id>` na zamčený: `Heslo pro '<nazev>': ` (skrytě). Jeden pokus —
  špatné heslo → „Spatne heslo.", prázdný vstup/EOF → „Odemknuti
  zruseno."; úspěch → odemčen, aktivní, „Prepnuto na seznam '<X>'."
  Odemčení není vratné přes `u` (příkaz `v`, který odemykal, se
  nesnapshotuje; obyčejné přepnutí mezi odemčenými vratné zůstává).
- `n <nazev>`: validace + unikátnost → volba hesla 2× (EOF → „Zalozeni
  zruseno.") → nový odemčený aktivní seznam, ID = max+1.
- `d`, `j`, cíl `m`: jen odemčené — u zamčeného „Seznam '<X>' je zamceny,
  nejdriv ho odemkni (v <id>)." `d <id>`/`j` na zamčený totéž.
- `zh`: mění heslo **aktivního** seznamu (nová sůl + klíč, okamžité
  uložení toho souboru, pravdivá hláška). V přehledu → „Prepni na
  konkretni seznam."
- Přehled `v 0`, řazení, `c`, přesuny, složená ID: pracují jen nad
  odemčenými (zamčený cíl složeného ID → hláška o zamčení).
- Smazání posledního odemčeného seznamu: aktivním se stává přehled 0
  (nulová automatická náhrada „Ukoly" se ruší — nula odemčených je nyní
  platný stav; úkolové příkazy hlásí zamčení/prázdno, `n` založí nový).
  `smazatSeznam` se upraví: po smazání aktivního → první zbývající
  odemčený, jinak 0.

## Ukládání
`s`/`q`: pro každý odemčený seznam zapíše jeho soubor (vlastní klíč+sůl,
temp+rename, pravdivé hlášky — selhání kteréhokoli = chybová hláška /
exit 1 na konci). Poté smaže soubory seznamů odstraněných v tomto sezení
(sleduje se množina názvů odemčených od startu; název, který už není ve
stavu, se smaže — `u` vrácený seznam tedy o soubor nepřijde). Přejmenování
(`j`) při uložení zapíše nový soubor a starý smaže. Nakonec zapíše
`nastaveni.txt` (razeni + posledni = název aktivního; při aktivním
přehledu poslední aktivní seznam).

## Migrace (jednorázová)
Když je `seznamy/` prázdný a existuje `<data>/ukoly.txt` (nebo záložně
`./ukoly.txt` v cwd — starý formát před XDG): dotaz na dosavadní heslo
(smyčka jako dnes, podporuje i nešifrovaný pra-formát), `parsujSeznamy`
rozdělí obsah a každý seznam se uloží jako vlastní soubor **se stejným
heslem** (vlastní čerstvá sůl pro každý). `@razeni` a `@aktivni` se
přenesou do `nastaveni.txt`. Starý soubor se přejmenuje na
`ukoly.txt.stara` (záloha; cwd varianta se nechává být). Hláška shrne
počet vytvořených seznamů.

## Testy
- `zkontrolujNazevSeznamu` (prázdný, `;`, `/`, tečka na začátku, platný).
- `serializujNastaveni`/`parsujNastaveni` round-trip, chybějící/rozbité
  hodnoty → výchozí.
- `serializujSeznamy` s pahýlem (`;zamceno`) pro undo-porovnání.
- `smazatSeznam`: nové chování (poslední odemčený → aktivní 0; žádná
  auto-náhrada) — úprava stávajících testů.
- Per-list serializace = stávající `serializujUkoly` round-trip (beze změn).
- E2E: založení prvního seznamu, restart + Enter na předvyplněný název,
  odemčení druhého seznamu za běhu, špatné heslo, `m` mezi odemčenými,
  cíl zamčený, `d` posledního odemčeného → přehled, `zh` jednoho seznamu
  (ostatní otevírá staré heslo), migrace stávajícího souboru (vzniknou
  soubory, funguje staré heslo, `ukoly.txt.stara` existuje).

## Mimo rozsah
- Sdílení/synchronizace souborů mezi stroji.
- Zamknutí seznamu zpět za běhu („lock").
- Přejmenování/mazání zamčených seznamů.
- Obnova zapomenutého hesla (z principu nemožná).
