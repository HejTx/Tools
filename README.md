# todolist — šifrovaný TODO list v terminálu

Jednoduchý správce úkolů pro terminál. Každý seznam úkolů je samostatný
**trezor** — soubor šifrovaný vlastním heslem (libsodium: Argon2id +
XSalsa20-Poly1305). Seznam můžeš sdílet předáním jeho hesla; ostatní
seznamy zůstávají kryptograficky nedostupné.

```
Seznamy: [0] Vse (46.2%) | [1] Ukoly (zamceno) | >[2] todoDev (46.2%)<
=== todoDev === (razeni: termin)
ID: 3, Popis: Poslat na github, Dokonceno: Ne, Termin: 19/07/26
ID: 11, Popis: Seznam dokoncenych ukolu, Dokonceno: Ne, Termin: 26/07/26
ID: 1, Popis: Dokoncit update, Dokonceno: Ano

ukol: p pridat · o hotovo · r odebrat · e upravit · m presunout · t termin
seznam: n novy · v vybrat · j prejmenovat · d smazat · c uklidit
jine: u zpet · z razeni · s ulozit · zh heslo · q konec · h napoveda
>
```

## Funkce

- **Trezory:** každý seznam má vlastní heslo a vlastní šifrovaný soubor;
  odemyká se při startu nebo za běhu (`v <id>`), zamčené seznamy jsou
  vidět jen jménem.
- **Úkoly s termíny:** `t <id> dd/mm/yy`; prošlé úkoly se zobrazují
  červeně, hotové šedě.
- **Řazení:** `z` přepíná podle ID / podle termínu (hotové vždy nakonec,
  bez termínu za termínovanými); volba se pamatuje.
- **Přehled Vše (`v 0`):** všechny odemčené seznamy najednou; úkoly se
  adresují složeným ID `<seznam>.<ukol>` (např. `o 2.3`), které funguje
  odkudkoli.
- **Přesuny a úpravy:** `m` přesune úkol mezi seznamy, `e` upraví popis,
  `c` uklidí hotové.
- **Undo:** `u` vrátí poslední změnu (a `u` znovu ji zopakuje).
- **Bezpečné ukládání:** atomický zápis přes dočasný soubor; selhání
  uložení se poctivě hlásí; heslo lze změnit za běhu (`zh`).
- Nápověda `h` obsahuje manuál všech příkazů.

## Sestavení

Vyžaduje g++ (C++17) a libsodium:

```bash
sudo apt install libsodium-dev
make        # sestaví todolist.exe
make test   # sestaví a spustí testy
```

## Použití

```bash
./todolist.exe
```

Při startu zadáš **název seznamu** (Enter přijme naposledy otevřený)
a jeho **heslo**. Neexistující název nabídne založení nového seznamu.
Data se ukládají do `$XDG_DATA_HOME/todolist/` (výchozí
`~/.local/share/todolist/`):

```
todolist/
├── seznamy/<nazev>.txt   # jeden šifrovaný trezor na seznam
└── nastaveni.txt         # nešifrované předvolby (řazení, poslední seznam)
```

Pozn.: **názvy** seznamů jsou zároveň názvy souborů, takže šifrovaný je
obsah úkolů, nikoli jména seznamů. Starší jednosouborový formát
(`ukoly.txt`) se při prvním spuštění automaticky rozdělí na trezory.

## Vývoj

Psáno testově (plain-assert testy v `test_todolist.cpp`, exact-string
kontroly vykreslování). Návrhové dokumenty jednotlivých funkcí jsou
v `docs/superpowers/specs/`, implementační plány v
`docs/superpowers/plans/`.
