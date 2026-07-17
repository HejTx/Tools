# Design: Archiv hotových úkolů (todoDev #11 + #12)

Rozhodnutí uživatele: `c` přesouvá hotové do archivu (místo mazání),
obnovený úkol se vrací jako nehotový, příkazy `a` (zobrazit archiv)
a `ob <id>` (obnovit). Archiv je na seznam a šifruje se stejným heslem.

## Uložení
- Soubor `seznamy/<nazev>.hotove.txt` — stejný kontejner, stejná sůl
  a klíč jako trezor seznamu (dešifruje se týmž heslem při odemčení).
- Plaintext = řádky úkolů (`id;popis;done;termin;priorita`), IDčka
  archivní (nezávislá řada, `back().id + 1`).
- Zapisuje se při `s`/`q`/`zh` jen u odemčených seznamů: neprázdný
  archiv → zápis, prázdný → případný soubor se smaže. `zh` přešifruje
  seznam i archiv (obojí novou solí/klíčem; selhání → starý klíč platí).
- Mazání/přejmenování seznamu maže/řeší i `.hotove.txt` (úklidová
  smyčka v `ulozVse` maže oba soubory zmizelých názvů).
- **Sken při startu ignoruje** soubory končící `.hotove.txt` (nejsou to
  seznamy). `zkontrolujNazevSeznamu` nově zakazuje názvy končící na
  `.hotove` („Nazev nesmi koncit na '.hotove'.").
- Poškozený/nedešifrovatelný archiv při odemčení: soubor se přejmenuje
  na `<cesta>.poskozeny` (bajty se zachovají), archiv začíná prázdný,
  hláška na stdout.
- Refaktor: `ulozUkolyDoSouboru(ukoly, klic, sul, cesta)` (tělo
  dosavadního `ulozSeznamDoSouboru`); `ulozSeznamDoSouboru` je nad ním
  a archiv se ukládá touž funkcí.

## Model a operace
- `Seznam` dostane `std::vector<Task> archiv;` (plní se při odemčení).
- `archivujHotove(Seznam&)` → int: přesune hotové z `ukoly` do `archiv`
  (nová archivní ID, pořadí zachováno). Volá ho `c` (aktivní seznam;
  v přehledu 0 všechny odemčené). Hlášky `c` nově pravdivé:
  „Do archivu presunuto ukolu: X." / „Zadne hotove ukoly k presunuti."
- `obnovUkol(Seznam&, int archivId)` → bool: vyjme z archivu, přidá do
  `ukoly` s novým ID seznamu, `done = false`, termín a priorita
  zůstávají.
- `r` zůstává tvrdé smazání (bez archivace).

## Příkazy
- `a` (`TypPrikazu::Archiv`, bez argumentů): modální stránka jako `h` —
  smaže obrazovku, `vytiskniArchiv(out, seznam, dnes)` vypíše
  `=== Archiv: <nazev> ===`, archivní úkoly přes `vytiskniUkol`
  (hotové = šedě), prázdný → „Zadny archivovany ukol.", pak „Pokracuj
  stiskem Enteru..." a jeden zahozený `getline` (EOF = konec + uložení).
  V přehledu 0 → „Prepni na konkretni seznam."
- `ob <id>` (`TypPrikazu::Obnovit`, `nactiIdArgument` — bez složených
  ID, pracuje nad aktivním seznamem): úspěch → „Ukol obnoven.",
  jinak „Ukol s ID x v archivu nenalezen.", v přehledu 0 → „Prepni na
  konkretni seznam."

## Undo
`serializujSeznamy` (jen pro porovnání změn a testy) přidá za úkoly
každého seznamu archivní řádky s prefixem `~`; `parsujSeznamy` řádky
začínající `~` přeskakuje. `Obnovit` a archivující `c` jsou tím vratné
přes `u`; `Archiv` (prohlížení) se přidá k vyloučeným typům vedle
`Napoveda`.

## Footer a nápověda
Footer, 2. řádek (vejde se do 80):
```
      m presunout · t termin · pr priorita · c uklidit · a archiv · ob obnovit
```
Nápověda: řádek `c` se zkrátí na
`  c                Presune hotove ukoly do archivu (v prehledu 0 vsude).`
a pod něj přibude
`  a                Zobrazi archiv hotovych ukolu aktivniho seznamu.`
`  ob <id>          Obnovi ukol z archivu (vrati se jako nehotovy).`

## Testy
- `archivujHotove` (počet, nové ID řady, pořadí, prázdný), `obnovUkol`
  (nové ID v seznamu, done=false, termín/priorita zachovány, nenalezen).
- `zkontrolujNazevSeznamu(".hotove" suffix)`.
- `serializujSeznamy` s archivem (`~` řádky) + `parsujSeznamy` je
  přeskočí.
- `vytiskniArchiv` exact-string (s úkoly i prázdný).
- Parser: `a`, `ob 2`, `ob` → Neznamy.
- Aktualizace footeru/nápovědy ve stávajících exact testech.
- E2E: c → a ukáže archiv, restart + odemčení → archiv přežil, ob →
  úkol zpět jako nehotový, u vrátí c i ob, zh → archiv čitelný novým
  heslem, smazání seznamu odstraní i `.hotove.txt`, sken nebere
  `.hotove.txt` jako seznam.

## Mimo rozsah
- Archiv v přehledu 0 (souhrnný pohled přes seznamy).
- Promazávání archivu (trvalé mazání z archivu).
- Složená ID pro `ob`.
