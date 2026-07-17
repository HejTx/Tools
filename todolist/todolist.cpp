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

std::string cestaArchivu(const std::string& adresar, const std::string& nazev) {
    return cestaSeznamu(adresar, nazev + ".hotove");
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

bool jeGitRepo(const std::string& adresar) {
    return std::filesystem::exists(
        std::filesystem::path(adresar.empty() ? "." : adresar) / ".git");
}

// Spustí git příkaz tiše; true = návratový kód 0.
bool spustGit(const std::string& adresar, const std::string& argumenty) {
    std::string prikaz = sestavGitPrikaz(adresar.empty() ? "." : adresar, argumenty)
                         + " >/dev/null 2>&1";
    return std::system(prikaz.c_str()) == 0;
}

// Stdout git příkazu; "" při chybě.
std::string vystupGitu(const std::string& adresar, const std::string& argumenty) {
    std::string prikaz = sestavGitPrikaz(adresar.empty() ? "." : adresar, argumenty)
                         + " 2>/dev/null";
    FILE* roura = popen(prikaz.c_str(), "r");
    if (!roura) return "";
    std::string vystup;
    char buf[256];
    while (fgets(buf, sizeof(buf), roura)) {
        vystup += buf;
    }
    pclose(roura);
    return vystup;
}

// Načte archiv seznamu týmž heslem; nedešifrovatelný soubor odloží stranou.
void nactiArchiv(Seznam& seznam, const std::string& cesta, const std::string& heslo) {
    std::optional<std::string> obsah = nactiObsahSouboru(cesta);
    if (!obsah) return;
    std::optional<VysledekDesifrovani> vysledek = desifruj(*obsah, heslo);
    if (!vysledek) {
        std::rename(cesta.c_str(), (cesta + ".poskozeny").c_str());
        std::cout << "Archiv " << cesta << " nelze desifrovat, odlozen jako .poskozeny.\n";
        return;
    }
    seznam.archiv = parsujUkoly(vysledek->plaintext);
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

    const bool synchronizace = jeGitRepo(adresar);
    if (synchronizace) {
        std::string gitignore = adresar.empty() ? ".gitignore" : adresar + "/.gitignore";
        if (!std::filesystem::exists(gitignore)) {
            std::ofstream gi(gitignore);
            gi << "nastaveni.txt\n*.tmp\n*.poskozeny\n";
        }
        if (!spustGit(adresar, "pull --ff-only --quiet")) {
            std::cout << "Synchronizace: pull se nezdaril"
                         " (offline, nebo mistni a vzdalene zmeny koliduji).\n";
        }
    }

    // sken trezorů -> pahýly (abecedně, ID 1..n)
    auto nactiPahyly = [&]() {
        std::vector<std::string> nazvy;
        std::error_code ec;
        std::string slozka = adresar.empty() ? "seznamy" : adresar + "/seznamy";
        for (const auto& polozka : std::filesystem::directory_iterator(slozka, ec)) {
            if (polozka.is_regular_file() && polozka.path().extension() == ".txt") {
                std::string jmeno = polozka.path().stem().string();
                // archivni soubory <nazev>.hotove.txt nejsou seznamy
                if (jmeno.size() >= 7 && jmeno.substr(jmeno.size() - 7) == ".hotove") continue;
                nazvy.push_back(jmeno);
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
            if (!vysledek) {
                sodium_memzero(heslo->data(), heslo->size());
                std::cout << "Spatne heslo nebo poskozeny soubor, zkus to znovu.\n";
                continue;
            }
            nactiArchiv(*existujici, cestaArchivu(adresar, nazev), *heslo);
            sodium_memzero(heslo->data(), heslo->size());
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
    bool syncSelhala = false;             // výsledek posledního commit+push
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
                std::string archivCesta = cestaArchivu(adresar, seznam.nazev);
                if (seznam.archiv.empty()) {
                    std::remove(archivCesta.c_str());
                } else if (!ulozUkolyDoSouboru(seznam.archiv, seznam.klic, seznam.sul,
                                               archivCesta)) {
                    ok = false;
                }
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
                if (!existuje) {
                    std::remove(cestaSeznamu(adresar, nazev).c_str());
                    std::remove(cestaArchivu(adresar, nazev).c_str());
                }
            }
        }
        nastaveni.razeni = stav.razeni;
        if (!posledniAktivni.empty()) nastaveni.posledni = posledniAktivni;
        std::ofstream vystup(cestaNastaveni(adresar), std::ios::binary);
        vystup << serializujNastaveni(nastaveni);

        syncSelhala = false;
        if (synchronizace && ok) {
            spustGit(adresar, "add -A");
            spustGit(adresar, "commit -m ulozeni --quiet");  // nic ke commitu != chyba
            if (!spustGit(adresar, "push --quiet")) {
                syncSelhala = true;
            }
        }
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
            prikaz.typ != TypPrikazu::Archiv && prikaz.typ != TypPrikazu::Verze &&
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
            case TypPrikazu::Priorita: {
                Seznam* cil = najdiCilUkolu(prikaz, zprava);
                if (!cil) break;
                int priorita = 2;
                if (!prikaz.popis.empty()) {
                    try {
                        priorita = std::stoi(prikaz.popis);
                    } catch (...) {
                        priorita = -1;
                    }
                }
                if (priorita < 1 || priorita > 3) {
                    zprava = "Neplatna priorita, pouzij 1-3.";
                } else if (!nastavPrioritu(cil->ukoly, prikaz.id, priorita)) {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                } else {
                    zprava = (priorita == 1)   ? "Priorita nastavena na vysokou."
                             : (priorita == 3) ? "Priorita nastavena na nizkou."
                                               : "Priorita nastavena na normalni.";
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
                        if (seznam.odemceno) pocet += archivujHotove(seznam);
                    }
                } else {
                    pocet = archivujHotove(*najdiSeznam(stav.seznamy, stav.aktivniId));
                }
                zprava = (pocet == 0)
                             ? "Zadne hotove ukoly k presunuti."
                             : "Do archivu presunuto ukolu: " + std::to_string(pocet) + ".";
                break;
            }
            case TypPrikazu::Archiv: {
                if (stav.aktivniId == 0) {
                    zprava = "Prepni na konkretni seznam.";
                    break;
                }
                std::cout << "\033[2J\033[H";
                vytiskniArchiv(std::cout, *najdiSeznam(stav.seznamy, stav.aktivniId),
                               dnesniKlic());
                std::string zahodit;
                if (!std::getline(std::cin, zahodit)) {
                    prikaz.typ = TypPrikazu::Konec;
                }
                zprava.clear();
                break;
            }
            case TypPrikazu::Obnovit: {
                if (stav.aktivniId == 0) {
                    zprava = "Prepni na konkretni seznam.";
                    break;
                }
                Seznam* aktivni = najdiSeznam(stav.seznamy, stav.aktivniId);
                if (obnovUkol(*aktivni, prikaz.id)) {
                    zprava = "Ukol obnoven.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " v archivu nenalezen.";
                }
                break;
            }
            case TypPrikazu::Ulozit:
                if (!ulozVse()) {
                    zprava = "CHYBA: Ulozeni se nezdarilo, zkus to znovu.";
                } else if (!synchronizace) {
                    zprava = "Ukoly ulozeny.";
                } else if (syncSelhala) {
                    zprava = "Ukoly ulozeny mistne, synchronizace se nezdarila (offline?).";
                } else {
                    zprava = "Ukoly ulozeny a synchronizovany.";
                }
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
                    if (!vysledek) {
                        sodium_memzero(heslo->data(), heslo->size());
                        zprava = "Spatne heslo.";
                        break;
                    }
                    nactiArchiv(*cil, cestaArchivu(adresar, cil->nazev), *heslo);
                    sodium_memzero(heslo->data(), heslo->size());
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
                bool ulozeno = ulozSeznamDoSouboru(*aktivni, cestaSeznamu(adresar, aktivni->nazev));
                if (ulozeno) {
                    std::string archivCesta = cestaArchivu(adresar, aktivni->nazev);
                    if (aktivni->archiv.empty()) {
                        std::remove(archivCesta.c_str());
                    } else {
                        ulozeno = ulozUkolyDoSouboru(aktivni->archiv, aktivni->klic,
                                                     aktivni->sul, archivCesta);
                    }
                }
                if (ulozeno) {
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
            case TypPrikazu::Verze: {
                if (!synchronizace) {
                    zprava = "Synchronizace neni zapnuta (datovy adresar neni git repozitar).";
                    break;
                }
                if (stav.aktivniId == 0) {
                    zprava = "Prepni na konkretni seznam.";
                    break;
                }
                Seznam* aktivni = najdiSeznam(stav.seznamy, stav.aktivniId);
                std::string relSoubor = "seznamy/" + aktivni->nazev + ".txt";
                std::vector<VerzeZaznam> verze = parsujVerze(vystupGitu(adresar,
                    "log --format=\"%h;%ad\" --date=format:\"%d/%m/%y %H:%M\" -- \"" + relSoubor + "\""));
                if (prikaz.id == -1) {
                    std::cout << "\033[2J\033[H";
                    vytiskniVerze(std::cout, aktivni->nazev, verze);
                    std::string zahodit;
                    if (!std::getline(std::cin, zahodit)) {
                        prikaz.typ = TypPrikazu::Konec;
                    }
                    zprava.clear();
                    break;
                }
                if (prikaz.id > static_cast<int>(verze.size())) {
                    zprava = "Verze " + std::to_string(prikaz.id) + " nenalezena.";
                    break;
                }
                // aktualni stav nejdriv na disk a do historie — i obnova je jen dalsi verzi
                ulozSeznamDoSouboru(*aktivni, cestaSeznamu(adresar, aktivni->nazev));
                std::string archivCesta = cestaArchivu(adresar, aktivni->nazev);
                if (aktivni->archiv.empty()) {
                    std::remove(archivCesta.c_str());
                } else {
                    ulozUkolyDoSouboru(aktivni->archiv, aktivni->klic, aktivni->sul, archivCesta);
                }
                spustGit(adresar, "add -A");
                spustGit(adresar, "commit -m pred-obnovou --quiet");
                const std::string& hash = verze[prikaz.id - 1].hash;
                if (!spustGit(adresar, "checkout " + hash + " -- \"" + relSoubor + "\"")) {
                    zprava = "Obnoveni verze se nezdarilo.";
                    break;
                }
                std::string relArchiv = "seznamy/" + aktivni->nazev + ".hotove.txt";
                if (spustGit(adresar, "cat-file -e " + hash + ":\"" + relArchiv + "\"")) {
                    spustGit(adresar, "checkout " + hash + " -- \"" + relArchiv + "\"");
                } else {
                    std::remove(archivCesta.c_str());
                }
                int seznamId = aktivni->id;
                aktivni->odemceno = false;
                aktivni->ukoly.clear();
                aktivni->archiv.clear();
                aktivni->klic.clear();
                stav.aktivniId = 0;
                for (const auto& seznam : stav.seznamy) {
                    if (seznam.odemceno) {
                        stav.aktivniId = seznam.id;
                        break;
                    }
                }
                zprava = "Verze obnovena; seznam je zamceny, odemkni ho pres v "
                         + std::to_string(seznamId) + ".";
                break;
            }
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
        if (synchronizace && syncSelhala) {
            std::cout << "\nUkoly ulozeny mistne (synchronizace se nezdarila). Nashledanou.\n";
        } else {
            std::cout << "\nUkoly ulozeny. Nashledanou.\n";
        }
        return 0;
    }
    std::cout << "\nPOZOR: Ukoly se nepodarilo ulozit!\n";
    return 1;
}
