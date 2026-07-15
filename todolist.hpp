#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

struct Task {
    int id;
    std::string description;
    bool done = false;
};

// Uloží úkoly do souboru ukoly.md
inline void ulozUkoly(const std::vector<Task>& ukoly, const std::string& soubor) {
    std::ofstream out(soubor);
    if (!out) {
        std::cerr << "Nepodarilo se otevrit soubor pro zapis: " << soubor << "\n";
        return;
    }

    for (const auto& ukol : ukoly) {
        out << ukol.id << ";" << ukol.description << ";" << ukol.done << "\n";
    }
}

// Načte úkoly ze souboru ukoly.md
inline std::vector<Task> nactiUkoly(const std::string& soubor) {
    std::vector<Task> ukoly;

    std::ifstream in(soubor);
    if (!in) {
        std::cerr << "Soubor " << soubor << " neexistuje, zacinam s prazdnym seznamem.\n";
        return ukoly;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string idStr, description, doneStr;

        std::getline(ss, idStr, ';');
        std::getline(ss, description, ';');
        std::getline(ss, doneStr, ';');

        Task ukol;
        ukol.id = std::stoi(idStr);
        ukol.description = description;
        ukol.done = (doneStr == "1");

        ukoly.push_back(ukol);
    }

    return ukoly;
}

inline void vytiskniUkol(std::ostream& out, const Task& ukol) {
    if (ukol.done) out << "\033[90m";
    out << "ID: " << ukol.id << ", Popis: " << ukol.description
        << ", Dokonceno: " << (ukol.done ? "Ano" : "Ne");
    if (ukol.done) out << "\033[0m";
    out << "\n";
}

inline void vytiskniUkoly(std::ostream& out, const std::vector<Task>& ukoly) {
    for (const auto& ukol : ukoly) {
        vytiskniUkol(out, ukol);
    }
}

inline bool pridatUkol(std::vector<Task>& ukoly, const std::string& description) {
    int newId = 1;
    if (!ukoly.empty()) {
        newId = ukoly.back().id + 1;
    }

    Task novyUkol;
    novyUkol.id = newId;
    novyUkol.description = description;
    novyUkol.done = false;

    ukoly.push_back(novyUkol);
    return true;
}

inline bool odebratUkol(std::vector<Task>& ukoly, int id) {
    auto it = std::remove_if(ukoly.begin(), ukoly.end(), [id](const Task& ukol) {
        return ukol.id == id;
    });

    if (it != ukoly.end()) {
        ukoly.erase(it, ukoly.end());
        return true;
    }
    return false;
}

inline bool oznacitUkolDokonceny(std::vector<Task>& ukoly, int id) {
    for (auto& ukol : ukoly) {
        if (ukol.id == id) {
            ukol.done = true;
            return true;
        }
    }
    return false;
}

enum class TypPrikazu { Pridat, Vypsat, Oznacit, Odebrat, Konec, Neznamy, Ulozit };

struct Prikaz {
    TypPrikazu typ = TypPrikazu::Neznamy;
    std::string popis;
    int id = -1;
};

inline Prikaz rozeberPrikaz(const std::string& radek) {
    Prikaz prikaz;
    std::istringstream ss(radek);
    std::string token;
    ss >> token;

    if (token == "q") {
        prikaz.typ = TypPrikazu::Konec;
    } else if (token == "v") {
        prikaz.typ = TypPrikazu::Vypsat;
    } else if (token == "s") {
        prikaz.typ = TypPrikazu::Ulozit;
    } else if (token == "p") {
        prikaz.typ = TypPrikazu::Pridat;
        std::string zbytek;
        std::getline(ss, zbytek);
        size_t start = zbytek.find_first_not_of(' ');
        prikaz.popis = (start == std::string::npos) ? "" : zbytek.substr(start);
    } else if (token == "o" || token == "r") {
        prikaz.typ = (token == "o") ? TypPrikazu::Oznacit : TypPrikazu::Odebrat;
        std::string idStr;
        if (ss >> idStr) {
            try {
                prikaz.id = std::stoi(idStr);
            } catch (...) {
                prikaz.typ = TypPrikazu::Neznamy;
            }
        } else {
            prikaz.typ = TypPrikazu::Neznamy;
        }
    } else {
        prikaz.typ = TypPrikazu::Neznamy;
    }

    return prikaz;
}

inline void vykresliObrazovku(std::ostream& out,
                              const std::vector<Task>& ukoly,
                              const std::string& zprava) {
    out << "=== Ukoly ===\n";
    if (ukoly.empty()) {
        out << "Zadne ukoly.\n";
    } else {
        vytiskniUkoly(out, ukoly);
    }
    out << "\n";
    if (!zprava.empty()) {
        out << zprava << "\n\n";
    }
    out << "Prikazy: p <popis> | o <id> | r <id> | s (ulozit) | q (ulozit a konec)\n"
        << "> ";
}
