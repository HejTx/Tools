# Design: Šifrování souboru ukoly.txt heslem

## Cíl
Data v `ukoly.txt` jsou dnes čitelný text. Nově se budou na disk ukládat
zašifrovaně; k rozšifrování je potřeba heslo zadané při startu programu.
Použije se poctivá kryptografie (libsodium), ne obfuskace.

## Formát souboru
Binární kontejner:

| Pole       | Velikost | Obsah                                              |
|------------|----------|----------------------------------------------------|
| hlavička   | 8 B      | magické bajty `TODOENC1`                           |
| sůl        | 16 B     | `crypto_pwhash_SALTBYTES`, náhodná při vzniku      |
| nonce      | 24 B     | `crypto_secretbox_NONCEBYTES`, nová při každém zápisu |
| ciphertext | zbytek   | `crypto_secretbox` (XSalsa20-Poly1305) nad textovou serializací; obsahuje 16B MAC |

Plaintext uvnitř = dosavadní textová serializace (`id;popis;done`, řádek na
úkol). Soubor, který nezačíná `TODOENC1`, se považuje za starý nešifrovaný
formát.

## Kryptografie
- Knihovna: **libsodium** (`sudo apt install libsodium-dev`, linkování `-lsodium`).
- Odvození klíče: `crypto_pwhash` — Argon2id, `OPSLIMIT_INTERACTIVE` /
  `MEMLIMIT_INTERACTIVE`, 32B klíč.
- Šifrování: `crypto_secretbox_easy` / otevírání `crypto_secretbox_open_easy`
  (autentizované — špatné heslo i poškozený soubor spolehlivě selžou).
- `sodium_init()` při startu; selhání → chyba na stderr a konec programu.
- Heslo se po odvození klíče vymaže z paměti (`sodium_memzero`). Klíč a sůl
  se drží v paměti po celý běh; každý zápis použije čerstvou náhodnou nonce.

## Nový modul `sifrovani.hpp`
Header-only, čisté funkce bez terminálového vstupu (testovatelné):

- `bool jeSifrovany(const std::string& obsah)` — kontrola magické hlavičky.
- `std::vector<unsigned char> odvodKlic(const std::string& heslo, const unsigned char* sul)`
  — Argon2id, 32B klíč.
- `std::string zasifruj(const std::string& plaintext, const unsigned char* klic, const unsigned char* sul)`
  — vrátí celý obsah souboru (hlavička + sůl + nová náhodná nonce + ciphertext).
- `std::optional<VysledekDesifrovani> desifruj(const std::string& obsahSouboru, const std::string& heslo)`
  — rozparsuje kontejner, odvodí klíč ze soli v souboru, ověří a dešifruje;
  `std::nullopt` = špatné heslo nebo poškozený/zkrácený soubor. Struktura
  `VysledekDesifrovani { std::string plaintext; std::vector<unsigned char> klic; std::array<unsigned char, crypto_pwhash_SALTBYTES> sul; }`
  vrací kromě plaintextu i odvozený klíč a sůl, aby je `main()` mohl použít
  pro následná uložení bez opětovného dotazu na heslo.

## Změny v `todolist.hpp`
Serializace se oddělí od souborového I/O:

- `std::string serializujUkoly(const std::vector<Task>&)` — dnešní tělo
  `ulozUkoly` nad stringem.
- `std::vector<Task> parsujUkoly(const std::string&)` — dnešní tělo
  `nactiUkoly` nad stringem.
- `ulozUkoly`/`nactiUkoly` se přepíšou jako kompozice: serializace ↔ šifrování
  ↔ disk (čtení/zápis v binárním režimu). Podpisy dostanou navíc klíč a sůl.

## Tok při startu (`main()`)
1. **Soubor neexistuje** → oznámení „zakládá se nový seznam", heslo 2× skrytě;
   neshodují-li se, opakovat. Vygeneruje se náhodná sůl, odvodí klíč, začne se
   s prázdným seznamem.
2. **Soubor existuje, šifrovaný** → dotaz na heslo (skrytě). Špatné heslo →
   hláška a nový dotaz, dokud uspěje. EOF (Ctrl+D) kdykoli při zadávání →
   čistý konec bez zápisu.
3. **Soubor existuje, nešifrovaný (migrace)** → načte se postaru
   (`parsujUkoly`), oznámí se, že soubor bude nově šifrovaný, heslo 2× jako
   v bodě 1. Při nejbližším uložení se zapíše šifrovaně. Stávající úkoly se
   zachovají.

Skryté zadávání hesla: pomocná funkce v `todolist.cpp` (termios, vypnuté echo,
po Enteru odřádkování). Není v `sifrovani.hpp` a netestuje se automatizovaně.

## Ukládání za běhu
Příkazy `s` a `q`/EOF fungují jako dosud, jen `ulozUkoly` zapisuje kontejner
(stejný klíč + sůl, nová nonce). Chyba zápisu → hláška na stderr jako dnes.

## Testy (`test_todolist.cpp`, linkování `-lsodium`)
- Round-trip: `zasifruj` → `desifruj` se stejným heslem vrátí originál.
- Špatné heslo → `std::nullopt`.
- Poškozený ciphertext (přepsaný bajt) → `std::nullopt`.
- Zkrácený/prázdný vstup → `std::nullopt` (žádný pád).
- `jeSifrovany`: true pro výstup `zasifruj`, false pro starý textový formát.
- Round-trip serializace: `parsujUkoly(serializujUkoly(x)) == x`.
- Stávající testy parseru a renderu beze změny.

## Build
- Jednorázově: `sudo apt install libsodium-dev`.
- Kompilace: `g++ todolist.cpp -o todolist.exe -lsodium`,
  `g++ test_todolist.cpp -o test_todolist.exe -lsodium`.

## Mimo rozsah
- Změna hesla za běhu (řeší se smazáním souboru nebo budoucím příkazem).
- Omezení počtu pokusů o heslo.
- Šifrování jednotlivých položek, částečné čtení bez hesla.
- Ochrana paměti za běhu nad rámec `sodium_memzero` hesla.
