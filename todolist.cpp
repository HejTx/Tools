#include "todolist.hpp"

#include <termios.h>
#include <unistd.h>

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
            std::cout << "Hesla se neshoduji, zkus to znovu.\n";
        } else if (prvni->empty()) {
            std::cout << "Heslo nesmi byt prazdne.\n";
        } else {
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

int main() {
    if (sodium_init() < 0) {
        std::cerr << "Nepodarilo se inicializovat libsodium.\n";
        return 1;
    }

    const std::string soubor = "ukoly.txt";

    std::vector<Task> ukoly;
    std::vector<unsigned char> klic;
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul{};

    std::optional<std::string> obsah = nactiObsahSouboru(soubor);
    if (!obsah) {
        std::cout << "Soubor " << soubor << " neexistuje, zaklada se novy sifrovany seznam.\n";
        if (!nastavNovyKlic(klic, sul)) return 0;
    } else if (jeSifrovany(*obsah)) {
        while (true) {
            std::optional<std::string> heslo = prectiHeslo("Heslo: ");
            if (!heslo) return 0;
            std::optional<VysledekDesifrovani> vysledek = desifruj(*obsah, *heslo);
            sodium_memzero(heslo->data(), heslo->size());
            if (vysledek) {
                ukoly = parsujUkoly(vysledek->plaintext);
                klic = std::move(vysledek->klic);
                sul = vysledek->sul;
                break;
            }
            std::cout << "Spatne heslo nebo poskozeny soubor, zkus to znovu.\n";
        }
    } else {
        std::cout << "Soubor " << soubor << " je v nesifrovanem formatu a bude zasifrovan.\n";
        ukoly = parsujUkoly(*obsah);
        if (!nastavNovyKlic(klic, sul)) return 0;
    }

    std::string radek;
    std::string zprava;
    while (true) {
        std::cout << "\033[2J\033[H";
        vykresliObrazovku(std::cout, ukoly, zprava);
        if (!std::getline(std::cin, radek)) break;

        Prikaz prikaz = rozeberPrikaz(radek);
        if (prikaz.typ == TypPrikazu::Konec) break;

        switch (prikaz.typ) {
            case TypPrikazu::Pridat:
                if (prikaz.popis.find(';') != std::string::npos) {
                    zprava = "Popis nesmi obsahovat znak ';'.";
                } else {
                    pridatUkol(ukoly, prikaz.popis);
                    zprava = "Ukol pridan.";
                }
                break;
            case TypPrikazu::Oznacit:
                if (oznacitUkolDokonceny(ukoly, prikaz.id)) {
                    zprava = "Ukol " + std::to_string(prikaz.id) + " oznacen jako hotovy.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            case TypPrikazu::Odebrat:
                if (odebratUkol(ukoly, prikaz.id)) {
                    zprava = "Ukol " + std::to_string(prikaz.id) + " odebran.";
                } else {
                    zprava = "Ukol s ID " + std::to_string(prikaz.id) + " nenalezen.";
                }
                break;
            case TypPrikazu::Ulozit:
                ulozUkoly(ukoly, soubor, klic, sul);
                zprava = "Ukoly ulozeny.";
                break;
            case TypPrikazu::Konec:
                break;
            default:
                zprava = "Neznamy prikaz.";
                break;
        }
    }

    ulozUkoly(ukoly, soubor, klic, sul);
    std::cout << "\nUkoly ulozeny. Nashledanou.\n";
    return 0;
}
