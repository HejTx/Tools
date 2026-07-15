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

inline void vytiskniUkol(const Task& ukol) {
    std::cout << "ID: " << ukol.id << ", Popis: " << ukol.description << ", Dokonceno: " << (ukol.done ? "Ano" : "Ne") << "\n";
}

inline void vytiskniUkoly(const std::vector<Task>& ukoly) {
    for (const auto& ukol : ukoly) {
        vytiskniUkol(ukol);
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
