#include "todolist.hpp"

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <set>

// Datový adresář ($XDG_DATA_HOME|~/.local/share)/todolist, "" = fallback na cwd.
std::string datovyAdresar() {
    std::filesystem::path zaklad;
    const char* xdg = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    if (xdg && xdg[0] == '/') {  // relativní XDG_DATA_HOME se dle XDG spec ignoruje
        zaklad = xdg;
    } else if (home && *home) {
        zaklad = std::filesystem::path(home) / ".local" / "share";
    } else {
        return "";
    }
    std::filesystem::path adresar = zaklad / "todolist";
    std::error_code ec;
    std::filesystem::create_directories(adresar / "seznamy", ec);
    if (ec) return "";
    return adresar.string();
}

std::string cestaSeznamu(const std::string& adresar, const std::string& nazev) {
    if (adresar.empty()) return "seznamy/" + nazev + ".txt";
    return adresar + "/seznamy/" + nazev + ".txt";
}

std::string cestaNastaveni(const std::string& adresar) {
    if (adresar.empty()) return "nastaveni.txt";
    return adresar + "/nastaveni.txt";
}

// Dnešní datum jako klíč rr*10000+mm*100+dd (formát klicTerminu).
int dnesniKlic() {
    std::time_t ted = std::time(nullptr);
    std::tm rozpad{};
    localtime_r(&ted, &rozpad);
    return (rozpad.tm_year % 100) * 10000 + (rozpad.tm_mon + 1) * 100 + rozpad.tm_mday;
}

// Šířka terminálu; mimo terminál (pipe) nebo při chybě 80.
int sirkaTerminalu() {
    winsize ws{};
    if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) {
        return ws.ws_col;
    }
    return 80;
}

// Přečte řádek se skrytým echem (termios). Vrací nullopt při EOF.
// Když stdin není terminál (pipe v testech), čte normálně.
std::optional<std::string> prectiHeslo(const std::string& vyzva) {
    std::cout << vyzva << std::flush;

    termios puvodni;
    bool jeTerminal = (tcgetattr(STDIN_FILENO, &puvodni) == 0);
    if (jeTerminal) {
        termios bezEcha = puvodni;
        bezEcha.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &bezEcha);
    }

    std::string heslo;
    bool nacteno = static_cast<bool>(std::getline(std::cin, heslo));

    if (jeTerminal) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &puvodni);
        std::cout << "\n";
    }

    if (!nacteno) return std::nullopt;
    return heslo;
}

// Dvakrát se zeptá na nové heslo; opakuje, dokud se zadání neshodují
// a nejsou prázdná. nullopt = EOF.
std::optional<std::string> zvolNoveHeslo() {
    while (true) {
        std::optional<std::string> prvni = prectiHeslo("Zvol nove heslo: ");
        if (!prvni) return std::nullopt;
        std::optional<std::string> druhe = prectiHeslo("Heslo znovu: ");
        if (!druhe) return std::nullopt;

        if (*prvni != *druhe) {
            sodium_memzero(prvni->data(), prvni->size());
            sodium_memzero(druhe->data(), druhe->size());
            std::cout << "Hesla se neshoduji, zkus to znovu.\n";
        } else if (prvni->empty()) {
            std::cout << "Heslo nesmi byt prazdne.\n";
        } else {
            sodium_memzero(druhe->data(), druhe->size());
            return prvni;
        }
    }
}

// Zvolí nové heslo, vygeneruje čerstvou sůl a odvodí klíč. false = EOF.
bool nastavNovyKlic(std::vector<unsigned char>& klic,
                    std::array<unsigned char, crypto_pwhash_SALTBYTES>& sul) {
    std::optional<std::string> heslo = zvolNoveHeslo();
    if (!heslo) return false;
    randombytes_buf(sul.data(), sul.size());
    klic = odvodKlic(*heslo, sul.data());
    sodium_memzero(heslo->data(), heslo->size());
    return true;
}

// Rozdělí starý víceseznamový soubor na trezory se stejným heslem.
// true = hotovo, false = zrušeno (EOF) nebo selhání zápisu.
bool migrujStaryFormat(const std::string& staraCesta, const std::string& adresar,
                       Nastaveni& nastaveni, bool prejmenovat) {
    std::optional<std::string> obsah = nactiObsahSouboru(staraCesta);
    if (!obsah) return false;

    std::cout << "Nalezen soubor stareho formatu: " << staraCesta
              << " - rozdeli se na samostatne sifrovane seznamy.\n";

    std::string plaintext;
    std::optional<std::string> heslo;
    if (jeSifrovany(*obsah)) {
        while (true) {
            heslo = prectiHeslo("Dosavadni heslo: ");
            if (!heslo) return false;
            std::optional<VysledekDesifrovani> vysledek = desifruj(*obsah, *heslo);
            if (vysledek) {
                plaintext = vysledek->plaintext;
                break;
            }
            sodium_memzero(heslo->data(), heslo->size());
            std::cout << "Spatne heslo nebo poskozeny soubor, zkus to znovu.\n";
        }
    } else {
        std::cout << "Soubor je nesifrovany; zvol heslo pro vsechny seznamy.\n";
        heslo = zvolNoveHeslo();
        if (!heslo) return false;
        plaintext = *obsah;
    }

    StavSeznamu stary = parsujSeznamy(plaintext);
    // Staré názvy nemusely být unikátní ani bezpečné pro souborový systém
    // (dřív se povolovalo '/' i tečka na začátku) — sanitizace + deduplikace.
    std::set<std::string> pouzite;
    int vytvoreno = 0;
    for (auto& seznam : stary.seznamy) {
        for (auto& znak : seznam.nazev) {
            if (znak == '/') znak = '_';
        }
        if (seznam.nazev.empty()) seznam.nazev = "Seznam";
        if (seznam.nazev[0] == '.') seznam.nazev[0] = '_';
        std::string zaklad = seznam.nazev;
        for (int poradi = 2; pouzite.count(seznam.nazev) > 0; ++poradi) {
            seznam.nazev = zaklad + " (" + std::to_string(poradi) + ")";
        }
        pouzite.insert(seznam.nazev);

        randombytes_buf(seznam.sul.data(), seznam.sul.size());
        seznam.klic = odvodKlic(*heslo, seznam.sul.data());
        if (!ulozSeznamDoSouboru(seznam, cestaSeznamu(adresar, seznam.nazev))) {
            sodium_memzero(heslo->data(), heslo->size());
            return false;
        }
        ++vytvoreno;
        if (seznam.id == stary.aktivniId) nastaveni.posledni = seznam.nazev;
    }
    sodium_memzero(heslo->data(), heslo->size());
    nastaveni.razeni = stary.razeni;

    if (prejmenovat) {
        std::rename(staraCesta.c_str(), (staraCesta + ".stara").c_str());
    }
    std::cout << "Vytvoreno seznamu: " << vytvoreno << " (vsechny se stejnym heslem).\n";
    return true;
}

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Nepodarilo se inicializovat libsodium.\n";
        return 1;
    }

    const std::string adresar = datovyAdresar();
    if (adresar.empty()) {
        std::error_code ec;
        std::filesystem::create_directories("seznamy", ec);
    }
    Nastaveni nastaveni =
        parsujNastaveni(nactiObsahSouboru(cestaNastaveni(adresar)).value_or(""));

    // sken trezorů -> pahýly (abecedně, ID 1..n)
    auto nactiPahyly = [&]() {
        std::vector<std::string> nazvy;
        std::error_code ec;
        std::string slozka = adresar.empty() ? "seznamy" : adresar + "/seznamy";
        for (const auto& polozka : std::filesystem::directory_iterator(slozka, ec)) {
            if (polozka.is_regular_file() && polozka.path().extension() == ".txt") {
                nazvy.push_back(polozka.path().stem().string());
            }
        }
        std::sort(nazvy.begin(), nazvy.end());
        std::vector<Seznam> pahyly;
        for (size_t i = 0; i < nazvy.size(); ++i) {
            Seznam pahyl;
            pahyl.id = static_cast<int>(i) + 1;
            pahyl.nazev = nazvy[i];
            pahyl.odemceno = false;
            pahyly.push_back(std::move(pahyl));
        }
        return pahyly;
    };

    std::vector<Seznam> pahyly = nactiPahyly();
    if (pahyly.empty()) {
        std::string staraData = adresar.empty() ? "ukoly.txt" : adresar + "/ukoly.txt";
        if (nactiObsahSouboru(staraData)) {
            if (!migrujStaryFormat(staraData, adresar, nastaveni, true)) return 0;
        } else if (!adresar.empty() && nactiObsahSouboru("ukoly.txt")) {
            if (!migrujStaryFormat("ukoly.txt", adresar, nastaveni, false)) return 0;
        }
        pahyly = nactiPahyly();
    }

    StavSeznamu stav;
    stav.seznamy = std::move(pahyly);
    stav.razeni = nastaveni.razeni;

    // volba prvního seznamu (jméno -> heslo; neexistující -> založení);
    // předvyplňuje se jen název, který na disku pořád existuje
    bool posledniExistuje = false;
    for (const auto& seznam : stav.seznamy) {
        if (seznam.nazev == nastaveni.posledni) posledniExistuje = true;
    }
    if (!posledniExistuje) nastaveni.posledni.clear();
    while (true) {
        std::string vyzva = nastaveni.posledni.empty()
                                ? "Seznam: "
                                : "Seznam [" + nastaveni.posledni + "]: ";
        std::cout << vyzva << std::flush;
        std::string nazev;
        if (!std::getline(std::cin, nazev)) return 0;
        if (nazev.empty()) nazev = nastaveni.posledni;
        std::string chyba = zkontrolujNazevSeznamu(nazev);
        if (!chyba.empty()) {
            std::cout << chyba << "\n";
            continue;
        }

        Seznam* existujici = nullptr;
        for (auto& seznam : stav.seznamy) {
            if (seznam.nazev == nazev) existujici = &seznam;
        }

        if (existujici) {
            std::optional<std::string> obsah =
                nactiObsahSouboru(cestaSeznamu(adresar, nazev));
            if (!obsah) {
                std::cout << "Soubor seznamu nelze precist.\n";
                continue;
            }
            std::optional<std::string> heslo = prectiHeslo("Heslo: ");
            if (!heslo) return 0;
            std::optional<VysledekDesifrovani> vysledek = desifruj(*obsah, *heslo);
            sodium_memzero(heslo->data(), heslo->size());
            if (!vysledek) {
                std::cout << "Spatne heslo nebo poskozeny soubor, zkus to znovu.\n";
                continue;
            }
            existujici->ukoly = parsujUkoly(vysledek->plaintext);
            existujici->klic = std::move(vysledek->klic);
            existujici->sul = vysledek->sul;
            existujici->odemceno = true;
            stav.aktivniId = existujici->id;
            break;
        }

        std::cout << "Seznam '" << nazev << "' neexistuje. Zalozit? (a/n) " << std::flush;
        std::string odpoved;
        if (!std::getline(std::cin, odpoved)) return 0;
        if (odpoved != "a") continue;
        Seznam novy;
        novy.id = stav.seznamy.empty() ? 1 : stav.seznamy.back().id + 1;
        novy.nazev = nazev;
        if (!nastavNovyKlic(novy.klic, novy.sul)) return 0;
        stav.seznamy.push_back(std::move(novy));
        stav.aktivniId = stav.seznamy.back().id;
        break;
    }

    std::string radek;
    std::string zprava;
    std::optional<StavSeznamu> predchozi;

    std::set<std::string> odemceneNazvy;  // pro mazání souborů odstraněných seznamů
    for (const auto& seznam : stav.seznamy) {
        if (seznam.odemceno) odemceneNazvy.insert(seznam.nazev);
    }
    std::string posledniAktivni = nastaveni.posledni;

    // Uloží všechny odemčené trezory, smaže soubory odstraněných seznamů
    // a zapíše nastavení. false = některý zápis selhal.
    auto ulozVse = [&]() -> bool {
        bool ok = true;
        for (const auto& seznam : stav.seznamy) {
            if (!seznam.odemceno) continue;
            if (ulozSeznamDoSouboru(seznam, cestaSeznamu(adresar, seznam.nazev))) {
                odemceneNazvy.insert(seznam.nazev);
            } else {
                ok = false;
            }
        }
        // Soubory odstraněných/přejmenovaných seznamů se mažou jen po úspěšném
        // zápisu všech trezorů — jinak by selhané přejmenování přišlo o data.
        if (ok) {
            for (const auto& nazev : odemceneNazvy) {
                bool existuje = false;
                for (const auto& seznam : stav.seznamy) {
                    if (seznam.nazev == nazev) existuje = true;
                }
                if (!existuje) std::remove(cestaSeznamu(adresar, nazev).c_str());
            }
        }
        nastaveni.razeni = stav.razeni;
        if (!posledniAktivni.empty()) nastaveni.posledni = posledniAktivni;
        std::ofstream vystup(cestaNastaveni(adresar), std::ios::binary);
        vystup << serializujNastaveni(nastaveni);
        return ok;
    };

    // Cílový seznam úkolového příkazu: složené ID > aktivní seznam; v přehledu
    // 0 je složené ID povinné. Při neúspěchu nastaví zprávu a vrátí nullptr.
    auto najdiCilUkolu = [&stav](const Prikaz& prikaz, std::string& zprava) -> Seznam* {
        Seznam* seznam = nullptr;
        if (prikaz.seznamUkolu != -1) {
            seznam = najdiSeznam(stav.seznamy, prikaz.seznamUkolu);
            if (!seznam) {
                zprava = "Seznam s ID " + std::to_string(prikaz.seznamUkolu) + " nenalezen.";
                return nullptr;
            }
        } else if (stav.aktivniId == 0) {
            zprava = "V prehledu pouzij ID ve tvaru <seznam>.<ukol>, napr. o 2.3.";
            return nullptr;
        } else {
            seznam = najdiSeznam(stav.seznamy, stav.aktivniId);
        }
        if (seznam && !seznam->odemceno) {
            zprava = "Seznam '" + seznam->nazev + "' je zamceny, nejdriv ho odemkni (v "
                     + std::to_string(seznam->id) + ").";
            return nullptr;
        }
        return seznam;
    };
    while (true) {
        if (stav.aktivniId != 0) {
            posledniAktivni = najdiSeznam(stav.seznamy, stav.aktivniId)->nazev;
        }
        std::cout << "\033[2J\033[H";
        vykresliObrazovku(std::cout, stav, zprava, sirkaTerminalu(), dnesniKlic());
        if (!std::getline(std::cin, radek)) break;

        Prikaz prikaz = rozeberPrikaz(radek);
        if (prikaz.typ == TypPrikazu::Konec) break;

        // Záloha stavu pro 'u'; ukládá se po switchi, jen když se stav změnil.
        bool sledovatZmenu =
            prikaz.typ != TypPrikazu::Zpet && prikaz.typ != TypPrikazu::Ulozit &&
            prikaz.typ != TypPrikazu::Napoveda && prikaz.typ != TypPrikazu::ZmenaHesla &&
            prikaz.typ != TypPrikazu::Neznamy;
        bool preskocSnapshot = false;
        StavSeznamu zaloha;
        if (sledovatZmenu) zaloha = stav;

        // Pozor: ukazatel na aktivní seznam se resolvuje až v jednotlivých
        // větvích — příkazy nad seznamy (push_back/erase) by ho zneplatnily.
        switch (prikaz.typ) {
            case TypPrikazu::Pridat:
                if (stav.aktivniId == 0) {
                    zprava = "V prehledu nelze pridavat, prepni na seznam.";
                } else if (prikaz.popis.find(';') != std::string::npos) {
                    zprava = "Popis nesmi obsahovat znak ';'.";
                } else {
                    pridatUkol(najdiSeznam(stav.seznamy, stav.aktivniId)->ukoly, prikaz.popis);
                    zprava = "Ukol pridan.";
                }
                break;
            case TypPrikazu::Oznacit: {
                Seznam* cil = najdiCilUkolu(prikaz, zprava);
                if (!cil) break;
                if (oznacitUkolDokonceny(cil->ukoly, prikaz.id)) {
                    zprava = "Ukol " + std::to_string(prikaz.id) + " oznacen jako hotovy.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            }
            case TypPrikazu::Odebrat: {
                Seznam* cil = najdiCilUkolu(prikaz, zprava);
                if (!cil) break;
                if (odebratUkol(cil->ukoly, prikaz.id)) {
                    zprava = "Ukol " + std::to_string(prikaz.id) + " odebran.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            }
            case TypPrikazu::UpravitUkol: {
                Seznam* cil = najdiCilUkolu(prikaz, zprava);
                if (!cil) break;
                if (prikaz.popis.empty()) {
                    zprava = "Popis nesmi byt prazdny.";
                } else if (prikaz.popis.find(';') != std::string::npos) {
                    zprava = "Popis nesmi obsahovat znak ';'.";
                } else if (upravitUkol(cil->ukoly, prikaz.id, prikaz.popis)) {
                    zprava = "Ukol upraven.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            }
            case TypPrikazu::Termin: {
                Seznam* cil = najdiCilUkolu(prikaz, zprava);
                if (!cil) break;
                if (!prikaz.popis.empty() && !jePlatnyTermin(prikaz.popis)) {
                    zprava = "Neplatny format terminu, pouzij dd/mm/yy.";
                } else if (!nastavTermin(cil->ukoly, prikaz.id, prikaz.popis)) {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                } else {
                    zprava = prikaz.popis.empty() ? "Termin odstranen."
                                                  : "Termin nastaven na " + prikaz.popis + ".";
                }
                break;
            }
            case TypPrikazu::PrepnoutRazeni:
                stav.razeni = (stav.razeni == 2) ? 1 : 2;
                zprava = (stav.razeni == 2) ? "Razeni: podle terminu." : "Razeni: podle ID.";
                break;
            case TypPrikazu::PresunoutUkol: {
                Seznam* zdroj = najdiCilUkolu(prikaz, zprava);
                if (!zdroj) break;
                Seznam* cilovySeznam = najdiSeznam(stav.seznamy, prikaz.id2);
                if (cilovySeznam && !cilovySeznam->odemceno) {
                    zprava = "Seznam '" + cilovySeznam->nazev + "' je zamceny, nejdriv ho odemkni (v "
                             + std::to_string(cilovySeznam->id) + ").";
                    break;
                }
                int zdrojId = (prikaz.seznamUkolu != -1) ? prikaz.seznamUkolu : stav.aktivniId;
                switch (presunUkol(stav, zdrojId, prikaz.id, prikaz.id2)) {
                    case 0:
                        zprava = "Ukol presunut do seznamu '"
                                 + najdiSeznam(stav.seznamy, prikaz.id2)->nazev + "'.";
                        break;
                    case 1:
                        zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                        break;
                    case 2:
                        zprava = "Seznam s ID " + std::to_string(prikaz.id2) + " nenalezen.";
                        break;
                    default:
                        zprava = "Ukol uz je v tomto seznamu.";
                        break;
                }
                break;
            }
            case TypPrikazu::VycistitHotove: {
                int pocet = 0;
                if (stav.aktivniId == 0) {
                    for (auto& seznam : stav.seznamy) {
                        pocet += vycistiHotove(seznam.ukoly);
                    }
                } else {
                    pocet = vycistiHotove(najdiSeznam(stav.seznamy, stav.aktivniId)->ukoly);
                }
                zprava = (pocet == 0)
                             ? "Zadne hotove ukoly k odstraneni."
                             : "Odstraneno hotovych ukolu: " + std::to_string(pocet) + ".";
                break;
            }
            case TypPrikazu::Ulozit:
                zprava = ulozVse()
                             ? "Ukoly ulozeny."
                             : "CHYBA: Ulozeni se nezdarilo, zkus to znovu.";
                break;
            case TypPrikazu::NovySeznam: {
                zprava = zkontrolujNazevSeznamu(prikaz.popis);
                if (!zprava.empty()) break;
                bool obsazeny = false;
                for (const auto& seznam : stav.seznamy) {
                    if (seznam.nazev == prikaz.popis) obsazeny = true;
                }
                if (obsazeny) {
                    zprava = "Seznam '" + prikaz.popis + "' uz existuje.";
                    break;
                }
                std::vector<unsigned char> novyKlic;
                std::array<unsigned char, crypto_pwhash_SALTBYTES> novaSul{};
                if (!nastavNovyKlic(novyKlic, novaSul)) {
                    zprava = "Zalozeni zruseno.";
                    break;
                }
                pridatSeznam(stav, prikaz.popis);
                stav.seznamy.back().klic = std::move(novyKlic);
                stav.seznamy.back().sul = novaSul;
                odemceneNazvy.insert(prikaz.popis);
                zprava = "Seznam '" + prikaz.popis + "' zalozen.";
                break;
            }
            case TypPrikazu::VybratSeznam: {
                if (prikaz.id == 0) {
                    vybratSeznam(stav, 0);
                    zprava = "Prepnuto na prehled 'Vse'.";
                    break;
                }
                Seznam* cil = najdiSeznam(stav.seznamy, prikaz.id);
                if (!cil) {
                    zprava = "Seznam s ID " + std::to_string(prikaz.id) + " nenalezen.";
                    break;
                }
                if (!cil->odemceno) {
                    preskocSnapshot = true;  // odemceni neni vratne pres u
                    std::optional<std::string> heslo =
                        prectiHeslo("Heslo pro '" + cil->nazev + "': ");
                    if (!heslo || heslo->empty()) {
                        zprava = "Odemknuti zruseno.";
                        break;
                    }
                    std::optional<std::string> obsah =
                        nactiObsahSouboru(cestaSeznamu(adresar, cil->nazev));
                    if (!obsah) {
                        sodium_memzero(heslo->data(), heslo->size());
                        zprava = "Soubor seznamu nelze precist.";
                        break;
                    }
                    std::optional<VysledekDesifrovani> vysledek = desifruj(*obsah, *heslo);
                    sodium_memzero(heslo->data(), heslo->size());
                    if (!vysledek) {
                        zprava = "Spatne heslo.";
                        break;
                    }
                    cil->ukoly = parsujUkoly(vysledek->plaintext);
                    cil->klic = std::move(vysledek->klic);
                    cil->sul = vysledek->sul;
                    cil->odemceno = true;
                    odemceneNazvy.insert(cil->nazev);
                }
                stav.aktivniId = cil->id;
                zprava = "Prepnuto na seznam '" + cil->nazev + "'.";
                break;
            }
            case TypPrikazu::PrejmenovatSeznam: {
                zprava = zkontrolujNazevSeznamu(prikaz.popis);
                if (!zprava.empty()) break;
                bool obsazeny = false;
                for (const auto& seznam : stav.seznamy) {
                    if (seznam.nazev == prikaz.popis) obsazeny = true;
                }
                if (obsazeny) {
                    zprava = "Seznam '" + prikaz.popis + "' uz existuje.";
                    break;
                }
                Seznam* cil = najdiSeznam(stav.seznamy, prikaz.id);
                if (!cil) {
                    zprava = "Seznam s ID " + std::to_string(prikaz.id) + " nenalezen.";
                } else if (!cil->odemceno) {
                    zprava = "Seznam '" + cil->nazev + "' je zamceny, nejdriv ho odemkni (v "
                             + std::to_string(cil->id) + ").";
                } else {
                    cil->nazev = prikaz.popis;
                    zprava = "Seznam prejmenovan na '" + prikaz.popis + "'.";
                }
                break;
            }
            case TypPrikazu::SmazatSeznam: {
                if (prikaz.id == -1 && stav.aktivniId == 0) {
                    zprava = "V prehledu neni aktivni seznam ke smazani.";
                    break;
                }
                int cil = (prikaz.id == -1) ? stav.aktivniId : prikaz.id;
                const Seznam* mazany = najdiSeznam(stav.seznamy, cil);
                if (!mazany) {
                    zprava = "Seznam s ID " + std::to_string(cil) + " nenalezen.";
                } else if (!mazany->odemceno) {
                    zprava = "Seznam '" + mazany->nazev + "' je zamceny, nejdriv ho odemkni (v "
                             + std::to_string(mazany->id) + ").";
                } else {
                    std::string nazev = mazany->nazev;
                    smazatSeznam(stav, cil);
                    zprava = "Seznam '" + nazev + "' smazan.";
                }
                break;
            }
            case TypPrikazu::ZmenaHesla: {
                if (stav.aktivniId == 0) {
                    zprava = "Prepni na konkretni seznam.";
                    break;
                }
                Seznam* aktivni = najdiSeznam(stav.seznamy, stav.aktivniId);
                std::vector<unsigned char> novyKlic;
                std::array<unsigned char, crypto_pwhash_SALTBYTES> novaSul{};
                if (!nastavNovyKlic(novyKlic, novaSul)) {
                    zprava = "Zmena hesla zrusena.";
                    break;
                }
                std::vector<unsigned char> staryKlic = aktivni->klic;
                std::array<unsigned char, crypto_pwhash_SALTBYTES> staraSul = aktivni->sul;
                aktivni->klic = std::move(novyKlic);
                aktivni->sul = novaSul;
                if (ulozSeznamDoSouboru(*aktivni, cestaSeznamu(adresar, aktivni->nazev))) {
                    zprava = "Heslo zmeneno a ulozeno.";
                } else {
                    // Nový klíč se zahazuje — jinak by pozdější úspěšné
                    // uložení tiše přešifrovalo soubor novým heslem.
                    aktivni->klic = std::move(staryKlic);
                    aktivni->sul = staraSul;
                    zprava = "CHYBA: Ulozeni se nezdarilo, plati stare heslo.";
                }
                break;
            }
            case TypPrikazu::Zpet:
                if (predchozi) {
                    std::swap(stav, *predchozi);
                    zprava = "Posledni zmena vracena. (u = znovu)";
                } else {
                    zprava = "Neni co vratit.";
                }
                break;
            case TypPrikazu::Napoveda: {
                std::cout << "\033[2J\033[H";
                vytiskniNapovedu(std::cout);
                std::string zahodit;
                if (!std::getline(std::cin, zahodit)) {
                    // EOF v nápovědě = konec jako v hlavní smyčce
                    prikaz.typ = TypPrikazu::Konec;
                }
                zprava.clear();
                break;
            }
            case TypPrikazu::Konec:
                break;
            default:
                zprava = "Neznamy prikaz.";
                break;
        }
        if (sledovatZmenu && !preskocSnapshot
            && serializujSeznamy(zaloha) != serializujSeznamy(stav)) {
            predchozi = std::move(zaloha);
        }
        if (prikaz.typ == TypPrikazu::Konec) break;
    }

    if (ulozVse()) {
        std::cout << "\nUkoly ulozeny. Nashledanou.\n";
        return 0;
    }
    std::cout << "\nPOZOR: Ukoly se nepodarilo ulozit!\n";
    return 1;
}
