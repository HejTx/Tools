#include "todolist.hpp"

int main() {
    const std::string soubor = "ukoly.txt";

    std::vector<Task> ukoly = nactiUkoly(soubor);

    ulozUkoly(ukoly, soubor);

    return 0;
}
