# Design: Priorita úkolu (todoDev #10)

Rozhodnutí uživatele: škála 1–3 (výchozí 2), příkaz `pr`, textový suffix
+ barevné zvýraznění vysoké priority.

## Model a formát
- `Task` dostane `int priorita = 2;` (1 = vysoká, 2 = normální, 3 = nízká).
- Serializace: 5. pole `id;popis;done;termin;priorita` (píše se vždy).
  Parsování: chybějící/nečíselné/mimo rozsah → 2 (staré 3- a 4-polové
  řádky zůstávají kompatibilní).

## Příkaz
- `pr <id> <1-3>` nastaví prioritu (funguje i složené ID `pr 2.3 1`),
  `pr <id>` bez hodnoty vrátí na 2. Parser: typ `Priorita`,
  `nactiUkolId` + zbytek řádku; hodnotu vyhodnocuje `main`.
- Hlášky: „Priorita nastavena na vysokou/normalni/nizkou.",
  mimo 1–3 → „Neplatna priorita, pouzij 1-3.", neexistující úkol →
  stávající hláška. Helper `nastavPrioritu(ukoly, id, priorita)` → bool.

## Řazení
Režim 2 (termín): komparátor `(done, klicTerminu, priorita, [seznamId,]
id)` — priorita je první tiebreaker mezi úkoly se stejným termínem
(i mezi úkoly bez termínu). Režim 1 (ID) beze změny.

## Zobrazení
- Suffix za termínem, jen mimo výchozí: `, Priorita: vysoka` /
  `, Priorita: nizka` (text se ukazuje i u hotových/prošlých).
- Vysoká priorita barví celý řádek žlutě (`\033[33m`). Přednost barev:
  **šedá (hotový) > červená (prošlý) > žlutá (vysoká)**.

## Footer a nápověda
Footer se přeskládá na 4 šedé řádky (úkolové příkazy na dvou, `c` se
vrací mezi úkolové; vše ≤ 80 sloupců):
```
ukol: p pridat · o hotovo · r odebrat · e upravit
      m presunout · t termin · pr priorita · c uklidit
seznam: n novy · v vybrat · j prejmenovat · d smazat
jine: u zpet · z razeni · s ulozit · zh heslo · q konec · h napoveda
```
Nápověda: pod `t` přibude
`  pr <id> <1-3>    Nastavi prioritu (1 vysoka, 2 normalni, 3 nizka).`
a řádek `c` se v manuálu nemění.

## Testy
- Serializace round-trip s prioritou + staré řádky → 2 (aktualizace
  exact-string testů na 5. pole).
- Parser: `pr 3 1`, `pr 2.3 1`, `pr 3` (reset), `pr` → Neznamy.
- Komparátory: stejný termín → vyšší priorita dřív; hotové stále
  nakonec; režim 1 ignoruje prioritu.
- Render: žlutý řádek; přednost šedá/červená/žlutá; suffix vysoká/nizka,
  nic pro 2; aktualizace footeru ve screen testech.
- E2E: nastavení, reset, pořadí při stejném termínu, barvy.

## Mimo rozsah
- Řazení podle priority jako samostatný režim `z`.
- Barevné odlišení nízké priority.
