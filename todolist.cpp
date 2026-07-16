#include "todolist.hpp"

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>

// Datový soubor v $XDG_DATA_HOME/todolist/, jinak ~/.local/share/todolist/.
// Bez obou proměnných (nebo při chybě) zůstává ukoly.txt v aktuálním adresáři.
std::string cestaKSouboru() {
    std::filesystem::path zaklad;
    const char* xdg = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    if (xdg && xdg[0] == '/') {  // relativní XDG_DATA_HOME se dle XDG spec ignoruje
        zaklad = xdg;
    } else if (home && *home) {
        zaklad = std::filesystem::path(home) / ".local" / "share";
    } else {
        return "ukoly.txt";
    }
    std::filesystem::path adresar = zaklad / "todolist";
    std::error_code ec;
    std::filesystem::create_directories(adresar, ec);
    if (ec) return "ukoly.txt";
    return (adresar / "ukoly.txt").string();
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

// Vrátí chybovou hlášku pro neplatný název seznamu, jinak prázdný string.
std::string zkontrolujNazev(const std::string& nazev) {
    if (nazev.empty()) return "Nazev nesmi byt prazdny.";
    if (nazev.find(';') != std::string::npos) return "Nazev nesmi obsahovat znak ';'.";
    return "";
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

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Nepodarilo se inicializovat libsodium.\n";
        return 1;
    }

    const std::string soubor = cestaKSouboru();

    StavSeznamu stav;
    std::vector<unsigned char> klic;
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul{};

    std::optional<std::string> obsah = nactiObsahSouboru(soubor);
    std::string zdroj = soubor;  // odkud data skutečně přišla (kvůli hláškám)
    if (!obsah && soubor != "ukoly.txt") {
        obsah = nactiObsahSouboru("ukoly.txt");
        if (obsah) {
            zdroj = "ukoly.txt";
            std::cout << "Soubor ukoly.txt z aktualniho adresare byl nacten; nove se uklada do "
                      << soubor << ".\n";
        }
    }
    if (!obsah) {
        std::cout << "Soubor " << soubor << " neexistuje, zaklada se novy sifrovany seznam.\n";
        stav = parsujSeznamy("");
        if (!nastavNovyKlic(klic, sul)) return 0;
    } else if (jeSifrovany(*obsah)) {
        while (true) {
            std::optional<std::string> heslo = prectiHeslo("Heslo: ");
            if (!heslo) return 0;
            std::optional<VysledekDesifrovani> vysledek = desifruj(*obsah, *heslo);
            sodium_memzero(heslo->data(), heslo->size());
            if (vysledek) {
                stav = parsujSeznamy(vysledek->plaintext);
                klic = std::move(vysledek->klic);
                sul = vysledek->sul;
                break;
            }
            std::cout << "Spatne heslo nebo poskozeny soubor, zkus to znovu.\n";
        }
    } else {
        std::cout << "Soubor " << zdroj << " je v nesifrovanem formatu a bude zasifrovan.\n";
        stav = parsujSeznamy(*obsah);
        if (!nastavNovyKlic(klic, sul)) return 0;
    }

    std::string radek;
    std::string zprava;
    std::optional<StavSeznamu> predchozi;
    while (true) {
        std::cout << "\033[2J\033[H";
        vykresliObrazovku(std::cout, stav, zprava, sirkaTerminalu());
        if (!std::getline(std::cin, radek)) break;

        Prikaz prikaz = rozeberPrikaz(radek);
        if (prikaz.typ == TypPrikazu::Konec) break;

        // Záloha stavu pro 'u'; ukládá se po switchi, jen když se stav změnil.
        bool sledovatZmenu =
            prikaz.typ != TypPrikazu::Zpet && prikaz.typ != TypPrikazu::Ulozit &&
            prikaz.typ != TypPrikazu::Napoveda && prikaz.typ != TypPrikazu::ZmenaHesla &&
            prikaz.typ != TypPrikazu::Neznamy;
        StavSeznamu zaloha;
        if (sledovatZmenu) zaloha = stav;

        // Pozor: ukazatel na aktivní seznam se resolvuje až v jednotlivých
        // větvích — příkazy nad seznamy (push_back/erase) by ho zneplatnily.
        switch (prikaz.typ) {
            case TypPrikazu::Pridat:
                if (prikaz.popis.find(';') != std::string::npos) {
                    zprava = "Popis nesmi obsahovat znak ';'.";
                } else {
                    pridatUkol(najdiSeznam(stav.seznamy, stav.aktivniId)->ukoly, prikaz.popis);
                    zprava = "Ukol pridan.";
                }
                break;
            case TypPrikazu::Oznacit:
                if (oznacitUkolDokonceny(najdiSeznam(stav.seznamy, stav.aktivniId)->ukoly, prikaz.id)) {
                    zprava = "Ukol " + std::to_string(prikaz.id) + " oznacen jako hotovy.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            case TypPrikazu::Odebrat:
                if (odebratUkol(najdiSeznam(stav.seznamy, stav.aktivniId)->ukoly, prikaz.id)) {
                    zprava = "Ukol " + std::to_string(prikaz.id) + " odebran.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            case TypPrikazu::UpravitUkol:
                if (prikaz.popis.empty()) {
                    zprava = "Popis nesmi byt prazdny.";
                } else if (prikaz.popis.find(';') != std::string::npos) {
                    zprava = "Popis nesmi obsahovat znak ';'.";
                } else if (upravitUkol(najdiSeznam(stav.seznamy, stav.aktivniId)->ukoly,
                                       prikaz.id, prikaz.popis)) {
                    zprava = "Ukol upraven.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            case TypPrikazu::PresunoutUkol:
                switch (presunUkol(stav, prikaz.id, prikaz.id2)) {
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
                        zprava = "Ukol uz je v aktivnim seznamu.";
                        break;
                }
                break;
            case TypPrikazu::VycistitHotove: {
                int pocet = vycistiHotove(najdiSeznam(stav.seznamy, stav.aktivniId)->ukoly);
                zprava = (pocet == 0)
                             ? "Zadne hotove ukoly k odstraneni."
                             : "Odstraneno hotovych ukolu: " + std::to_string(pocet) + ".";
                break;
            }
            case TypPrikazu::Ulozit:
                zprava = ulozSeznamy(stav, soubor, klic, sul)
                             ? "Ukoly ulozeny."
                             : "CHYBA: Ulozeni se nezdarilo, zkus to znovu.";
                break;
            case TypPrikazu::NovySeznam:
                zprava = zkontrolujNazev(prikaz.popis);
                if (zprava.empty()) {
                    pridatSeznam(stav, prikaz.popis);
                    zprava = "Seznam '" + prikaz.popis + "' zalozen.";
                }
                break;
            case TypPrikazu::VybratSeznam:
                if (vybratSeznam(stav, prikaz.id)) {
                    zprava = "Prepnuto na seznam '"
                             + najdiSeznam(stav.seznamy, stav.aktivniId)->nazev + "'.";
                } else {
                    zprava = "Seznam s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            case TypPrikazu::PrejmenovatSeznam:
                zprava = zkontrolujNazev(prikaz.popis);
                if (!zprava.empty()) {
                    break;
                }
                if (prejmenovatSeznam(stav.seznamy, prikaz.id, prikaz.popis)) {
                    zprava = "Seznam prejmenovan na '" + prikaz.popis + "'.";
                } else {
                    zprava = "Seznam s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            case TypPrikazu::SmazatSeznam: {
                int cil = (prikaz.id == -1) ? stav.aktivniId : prikaz.id;
                const Seznam* mazany = najdiSeznam(stav.seznamy, cil);
                if (mazany) {
                    std::string nazev = mazany->nazev;
                    smazatSeznam(stav, cil);
                    zprava = "Seznam '" + nazev + "' smazan.";
                } else {
                    zprava = "Seznam s ID " + std::to_string(cil) + " nenalezen.";
                }
                break;
            }
            case TypPrikazu::ZmenaHesla: {
                std::vector<unsigned char> novyKlic;
                std::array<unsigned char, crypto_pwhash_SALTBYTES> novaSul{};
                if (nastavNovyKlic(novyKlic, novaSul)) {
                    if (ulozSeznamy(stav, soubor, novyKlic, novaSul)) {
                        klic = std::move(novyKlic);
                        sul = novaSul;
                        zprava = "Heslo zmeneno a ulozeno.";
                    } else {
                        // Nový klíč se zahazuje — jinak by pozdější úspěšné
                        // uložení tiše přešifrovalo soubor novým heslem.
                        zprava = "CHYBA: Ulozeni se nezdarilo, plati stare heslo.";
                    }
                } else {
                    zprava = "Zmena hesla zrusena.";
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
        if (sledovatZmenu && serializujSeznamy(zaloha) != serializujSeznamy(stav)) {
            predchozi = std::move(zaloha);
        }
        if (prikaz.typ == TypPrikazu::Konec) break;
    }

    if (ulozSeznamy(stav, soubor, klic, sul)) {
        std::cout << "\nUkoly ulozeny. Nashledanou.\n";
        return 0;
    }
    std::cout << "\nPOZOR: Ukoly se nepodarilo ulozit!\n";
    return 1;
}
