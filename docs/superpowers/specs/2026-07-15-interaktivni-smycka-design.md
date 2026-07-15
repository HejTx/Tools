# Design: Interaktivní smyčka v main()

## Cíl
Nahradit pevně danou sekvenci volání v `main()` interaktivní smyčkou, která čte příkazy od uživatele, dokud nezadá `q`.

## Formát vstupu
Jeden řádek (`std::getline`), první token = příkaz:

- `p <popis>` — přidá úkol (`pridatUkol`), popis = zbytek řádku za "p "
- `v` — vypíše úkoly (`vytiskniUkoly`)
- `o <id>` — označí úkol s daným ID jako hotový (`oznacitUkolDokonceny`)
- `r <id>` — odebere úkol s daným ID (`odebratUkol`)
- `q` — ukončí smyčku

## Chování
1. Před prvním vstupem a po každé akci (kromě `q`) se vypíše krátká nápověda s dostupnými příkazy.
2. Neplatný příkaz nebo neplatné/neexistující ID → vypíše se chybová hláška, smyčka pokračuje.
3. Po `q` smyčka skončí a proběhne `ulozUkoly` (uloží se jednou, na konci, stejně jako dosud).
4. Parsování příkazu je jednoduchý if-else řetězec na první token, aby šlo snadno přidat další příkazy (např. `s` pro save) bez změny struktury smyčky.

## Mimo rozsah
- Validace formátu popisu u `p` (bere se celý zbytek řádku bez ořezávání).
- Perzistence po každé akci (ukládá se jen při ukončení, jako dosud).
