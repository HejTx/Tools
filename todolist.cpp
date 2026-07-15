#include "todolist.hpp"

int main() {
    const std::string soubor = "ukoly.txt";
    std::vector<Task> ukoly = nactiUkoly(soubor);

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
                ulozUkoly(ukoly, soubor);
                zprava = "Ukoly ulozeny.";
                break;
            case TypPrikazu::Konec:
                break;
            default:
                zprava = "Neznamy prikaz.";
                break;
        }
    }

    ulozUkoly(ukoly, soubor);
    std::cout << "\nUkoly ulozeny. Nashledanou.\n";
    return 0;
}
