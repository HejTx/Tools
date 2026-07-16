# Design: Více seznamů úkolů s přepínáním

## Cíl
Aplikace dnes drží jediný seznam úkolů. Nově půjde mít víc pojmenovaných
seznamů (např. „Nakup", „Ukoly EQ tyden") a přepínat mezi nimi. Všechny
seznamy žijí v jednom šifrovaném `ukoly.txt` pod jedním heslem — startovní
tok s heslem se nemění.

## Datový model
```cpp
struct Seznam {
    int id;                    // stabilní číselné ID pro příkazy
    std::string nazev;         // zobrazovaný název (bez ';')
    std::vector<Task> ukoly;
};
```
Stav aplikace: `std::vector<Seznam> seznamy` + `int aktivniId`.

- ID seznamu se přiděluje jako u úkolů: `seznamy.back().id + 1` (první = 1).
  ID je stabilní, dokud se seznam nesmaže; po smazání se čísla neposouvají.
- ID úkolů zůstávají per-seznam — každý seznam čísluje své úkoly od 1.
- Příkazy `p`, `o`, `r` pracují vždy jen s aktivním seznamem.
- Název seznamu nesmí být prázdný a nesmí obsahovat `;` (stejné pravidlo
  jako popis úkolu).

## Formát souboru
Plaintext uvnitř šifrovaného kontejneru (kontejner sám beze změny):

```
@aktivni;2
#seznam;1;Nakup
1;mleko;0
2;chleba;1
#seznam;2;Ukoly EQ tyden
1;report;0
```

- První řádek `@aktivni;<id>` — ID naposledy otevřeného seznamu; při startu
  se znovu otevře. Ukazuje-li na neexistující seznam, otevře se první.
- Řádek `#seznam;<id>;<nazev>` zahajuje seznam; následující řádky
  `id;popis;done` jsou jeho úkoly (dosavadní formát beze změny).
- **Migrace:** obsah, který nezačíná `@aktivni`, je starý jednoseznamový
  formát — načte se jako jeden seznam `Ukoly` s ID 1, který je aktivní.
  Při nejbližším uložení se zapíše nový formát.

## Obrazovka
Nad úkoly přibude řádek se všemi seznamy; u každého je procento hotových
úkolů (hotové/celkem, jedno desetinné místo; prázdný seznam = `0.0%`).
Aktivní seznam je zvýrazněn `>...<` a tučně (`\033[1m`):

```
Seznamy: [1] Nakup (50.0%) | \033[1m>[2] Ukoly EQ tyden (29.3%)<\033[0m | [3] Prace (0.0%)
=== Ukoly EQ tyden ===
ID: 1, Popis: report, Dokonceno: Ne

Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q
>
```

Titulek `=== ... ===` nese název aktivního seznamu místo dosavadního
statického `Ukoly`.

## Příkazy
| Příkaz | Akce |
|---|---|
| `n <nazev>` | Založí nový seznam a přepne na něj |
| `v <id>` | Přepne (vybere) seznam podle ID |
| `j <id> <nazev>` | Přejmenuje seznam |
| `d` | Smaže aktivní seznam |
| `d <id>` | Smaže seznam podle ID |

- Název u `n` a `j` je celý zbytek řádku (smí obsahovat mezery),
  s ořezanými úvodními mezerami — stejně jako popis u `p`.
- Chování `p/o/r/s/q` beze změny (nad aktivním seznamem; `s`/`q` ukládá
  všechny seznamy + `@aktivni`).
- Neexistující ID, prázdný název nebo název se `;` → hláška na stavovém
  řádku jako u dosavadních chyb.
- Po smazání aktivního seznamu se aktivním stane první zbývající.
- Smazání posledního zbývajícího seznamu je dovoleno — hned se založí
  čerstvý prázdný seznam `Ukoly`; aplikace nikdy nemá nula seznamů.
- Mazání je **bez potvrzení** (dle zadání) — rychlost před pojistkou.
- Parser: `d` bez argumentu je platný (id = -1 = aktivní); `v`/`j` bez
  argumentů zůstávají `Neznamy` (stávající test `rozeberPrikaz("v")` →
  `Neznamy` zůstává v platnosti).

## Změny v kódu
- `todolist.hpp`: struktura `Seznam`; `serializujSeznamy`/`parsujSeznamy`
  (čisté funkce nad stringem, uvnitř využijí dosavadní
  `serializujUkoly`/`parsujUkoly`); operace `pridatSeznam`, `odebratSeznam`,
  `prejmenovatSeznam`, `najdiSeznam`; formátování procent; `vykresliObrazovku`
  dostane `seznamy` + `aktivniId` místo jednoho vektoru úkolů; `rozeberPrikaz`
  rozšířen o `NovySeznam`, `VybratSeznam`, `PrejmenovatSeznam`, `SmazatSeznam`.
- `ulozUkoly` → ukládá výstup `serializujSeznamy` (jinak beze změny —
  temp soubor + rename, stejný klíč a sůl).
- `todolist.cpp` (`main`): stav `seznamy` + `aktivniId`, nové větve switche.
  Načítání: `parsujSeznamy` řeší i migraci starého formátu, takže větev
  „nešifrovaný soubor" i „šifrovaný soubor" volají stejnou funkci.

## Testy (`test_todolist.cpp`, stávající styl assertů)
- Round-trip `parsujSeznamy(serializujSeznamy(x))` pro víc seznamů,
  včetně prázdného seznamu a zapamatovaného `@aktivni`.
- Migrace: starý formát → jeden seznam `Ukoly`, ID 1, aktivní.
- Operace: založení (přidělení ID), přepnutí, přejmenování, smazání
  neaktivního, smazání aktivního (aktivace prvního zbývajícího), smazání
  posledního (vznik čerstvého `Ukoly`).
- Validace názvů: prázdný, obsahující `;`.
- Formátování procent: 0/0 → `0.0%`, 1/2 → `50.0%`, zaokrouhlení.
- Parser: `n`, `v 2`, `j 2 Prace`, `d`, `d 3`, chybné varianty.
- Render: aktualizované exact-string testy obrazovky (řádek seznamů,
  tučné zvýraznění, titulek s názvem, nová nápověda příkazů).

## Build
Beze změny: `g++ todolist.cpp -o todolist.exe -lsodium`,
`g++ test_todolist.cpp -o test_todolist.exe -lsodium`.

## Mimo rozsah
- Přesun úkolů mezi seznamy.
- Řazení / ruční pořadí seznamů.
- Zalamování řádku seznamů při velkém počtu seznamů.
- Samostatná hesla pro jednotlivé seznamy.
