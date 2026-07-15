#include "todolist.hpp"

void vypisNapovedu() {
    std::cout << "\nPrikazy:\n"
              << "  p <popis>  - pridat ukol\n"
              << "  v          - vypsat ukoly\n"
              << "  o <id>     - oznacit ukol jako hotovy\n"
              << "  r <id>     - odebrat ukol\n"
              << "  q          - konec\n"
              << "> ";
}

int main() {
    const std::string soubor = "ukoly.txt";
    std::vector<Task> ukoly = nactiUkoly(soubor);

    std::string radek;
    while (true) {
        vypisNapovedu();
        if (!std::getline(std::cin, radek)) break;

        Prikaz prikaz = rozeberPrikaz(radek);
        if (prikaz.typ == TypPrikazu::Konec) break;

        switch (prikaz.typ) {
            case TypPrikazu::Vypsat:
                vytiskniUkoly(ukoly);
                break;
            case TypPrikazu::Pridat:
                pridatUkol(ukoly, prikaz.popis);
                break;
            case TypPrikazu::Oznacit:
                if (!oznacitUkolDokonceny(ukoly, prikaz.id)) {
                    std::cout << "Ukol s ID " << prikaz.id << " nenalezen.\n";
                }
                break;
            case TypPrikazu::Odebrat:
                if (!odebratUkol(ukoly, prikaz.id)) {
                    std::cout << "Ukol s ID " << prikaz.id << " nenalezen.\n";
                }
                break;
            case TypPrikazu::Konec:
                break;
            default:
                std::cout << "Neznamy prikaz.\n";
                break;
        }
    }

    ulozUkoly(ukoly, soubor);
    return 0;
}
