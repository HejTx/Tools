#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>

#include "sifrovani.hpp"

struct Task {
    int id;
    std::string description;
    bool done = false;
};

// Podíl hotových úkolů jako "50.0%"; prázdný seznam = "0.0%".
inline std::string formatujProcenta(const std::vector<Task>& ukoly) {
    int hotovo = 0;
    for (const auto& ukol : ukoly) {
        if (ukol.done) ++hotovo;
    }
    double procenta = ukoly.empty() ? 0.0 : 100.0 * hotovo / ukoly.size();
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f%%", procenta);
    return buf;
}

inline std::string serializujUkoly(const std::vector<Task>& ukoly) {
    std::ostringstream out;
    for (const auto& ukol : ukoly) {
        out << ukol.id << ";" << ukol.description << ";" << ukol.done << "\n";
    }
    return out.str();
}

inline std::vector<Task> parsujUkoly(const std::string& obsah) {
    std::vector<Task> ukoly;

    std::istringstream in(obsah);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string idStr, description, doneStr;

        std::getline(ss, idStr, ';');
        std::getline(ss, description, ';');
        std::getline(ss, doneStr, ';');

        Task ukol;
        try {
            ukol.id = std::stoi(idStr);
        } catch (...) {
            continue;  // poškozený řádek se přeskočí, ať migrace nespadne
        }
        ukol.description = description;
        ukol.done = (doneStr == "1");

        ukoly.push_back(ukol);
    }

    return ukoly;
}

struct Seznam {
    int id;
    std::string nazev;
    std::vector<Task> ukoly;
};

struct StavSeznamu {
    std::vector<Seznam> seznamy;
    int aktivniId = 1;
};

inline Seznam* najdiSeznam(std::vector<Seznam>& seznamy, int id) {
    for (auto& seznam : seznamy) {
        if (seznam.id == id) return &seznam;
    }
    return nullptr;
}

inline const Seznam* najdiSeznam(const std::vector<Seznam>& seznamy, int id) {
    for (const auto& seznam : seznamy) {
        if (seznam.id == id) return &seznam;
    }
    return nullptr;
}

inline std::string serializujSeznamy(const StavSeznamu& stav) {
    std::ostringstream out;
    out << "@aktivni;" << stav.aktivniId << "\n";
    for (const auto& seznam : stav.seznamy) {
        out << "#seznam;" << seznam.id << ";" << seznam.nazev << "\n"
            << serializujUkoly(seznam.ukoly);
    }
    return out.str();
}

// Rozparsuje nový formát; obsah bez hlavičky @aktivni je starý jednoseznamový
// formát a zmigruje se na jeden seznam "Ukoly". Vždy vrátí aspoň jeden seznam
// a aktivniId ukazující na existující seznam.
inline StavSeznamu parsujSeznamy(const std::string& obsah) {
    StavSeznamu stav;

    if (obsah.rfind("@aktivni;", 0) != 0) {
        stav.seznamy.push_back({1, "Ukoly", parsujUkoly(obsah)});
        return stav;
    }

    std::istringstream in(obsah);
    std::string line;
    std::getline(in, line);
    try {
        stav.aktivniId = std::stoi(line.substr(std::string("@aktivni;").size()));
    } catch (...) {}

    std::string bufferUkolu;
    auto uzavriSeznam = [&]() {
        if (!stav.seznamy.empty()) {
            stav.seznamy.back().ukoly = parsujUkoly(bufferUkolu);
        }
        bufferUkolu.clear();
    };

    while (std::getline(in, line)) {
        if (line.rfind("#seznam;", 0) == 0) {
            uzavriSeznam();
            std::istringstream ss(line);
            std::string prefix, idStr, nazev;
            std::getline(ss, prefix, ';');
            std::getline(ss, idStr, ';');
            std::getline(ss, nazev);
            Seznam seznam;
            try {
                seznam.id = std::stoi(idStr);
            } catch (...) {
                seznam.id = stav.seznamy.empty() ? 1 : stav.seznamy.back().id + 1;
            }
            seznam.nazev = nazev;
            stav.seznamy.push_back(std::move(seznam));
        } else {
            bufferUkolu += line;
            bufferUkolu += "\n";
        }
    }
    uzavriSeznam();

    if (stav.seznamy.empty()) {
        stav.seznamy.push_back({1, "Ukoly", {}});
    }
    if (najdiSeznam(stav.seznamy, stav.aktivniId) == nullptr) {
        stav.aktivniId = stav.seznamy.front().id;
    }
    return stav;
}

// Založí seznam a rovnou na něj přepne. Vrací nové ID.
inline int pridatSeznam(StavSeznamu& stav, const std::string& nazev) {
    int noveId = stav.seznamy.empty() ? 1 : stav.seznamy.back().id + 1;
    stav.seznamy.push_back({noveId, nazev, {}});
    stav.aktivniId = noveId;
    return noveId;
}

inline bool vybratSeznam(StavSeznamu& stav, int id) {
    if (najdiSeznam(stav.seznamy, id) == nullptr) return false;
    stav.aktivniId = id;
    return true;
}

inline bool prejmenovatSeznam(std::vector<Seznam>& seznamy, int id, const std::string& nazev) {
    Seznam* seznam = najdiSeznam(seznamy, id);
    if (!seznam) return false;
    seznam->nazev = nazev;
    return true;
}

// Smaže seznam podle ID. Po smazání aktivního se aktivním stane první
// zbývající; po smazání posledního vznikne čerstvý prázdný seznam "Ukoly".
inline bool smazatSeznam(StavSeznamu& stav, int id) {
    auto it = std::find_if(stav.seznamy.begin(), stav.seznamy.end(),
                           [id](const Seznam& seznam) { return seznam.id == id; });
    if (it == stav.seznamy.end()) return false;
    stav.seznamy.erase(it);

    if (stav.seznamy.empty()) {
        stav.seznamy.push_back({1, "Ukoly", {}});
        stav.aktivniId = 1;
    } else if (najdiSeznam(stav.seznamy, stav.aktivniId) == nullptr) {
        stav.aktivniId = stav.seznamy.front().id;
    }
    return true;
}

// Uloží všechny seznamy zašifrovaně (stejný klíč a sůl, nová nonce při každém zápisu).
// Zapisuje přes dočasný soubor a rename, aby selhání zápisu nezničilo původní data.
// Vrací false, když se zápis nezdařil (původní soubor zůstává nedotčen).
inline bool ulozSeznamy(const StavSeznamu& stav, const std::string& soubor,
                        const std::vector<unsigned char>& klic,
                        const std::array<unsigned char, crypto_pwhash_SALTBYTES>& sul) {
    const std::string docasny = soubor + ".tmp";

    std::ofstream out(docasny, std::ios::binary);
    if (!out) {
        std::cerr << "Nepodarilo se otevrit soubor pro zapis: " << docasny << "\n";
        return false;
    }

    out << zasifruj(serializujSeznamy(stav), klic, sul);
    out.flush();
    if (!out) {
        std::cerr << "Zapis do souboru " << docasny << " se nezdaril, puvodni data zustavaji.\n";
        out.close();
        std::remove(docasny.c_str());
        return false;
    }
    out.close();

    if (std::rename(docasny.c_str(), soubor.c_str()) != 0) {
        std::cerr << "Nepodarilo se prejmenovat " << docasny << " na " << soubor << ".\n";
        std::remove(docasny.c_str());
        return false;
    }
    return true;
}

// Přečte celý soubor jako syrové bajty. nullopt = soubor nejde otevřít.
inline std::optional<std::string> nactiObsahSouboru(const std::string& soubor) {
    std::ifstream in(soubor, std::ios::binary);
    if (!in) return std::nullopt;

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
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

enum class TypPrikazu { Pridat, Oznacit, Odebrat, Konec, Neznamy, Ulozit,
                        NovySeznam, VybratSeznam, PrejmenovatSeznam, SmazatSeznam,
                        Napoveda };

struct Prikaz {
    TypPrikazu typ = TypPrikazu::Neznamy;
    std::string popis;
    int id = -1;
};

// Zbytek řádku bez úvodních mezer (popis úkolu / název seznamu).
inline std::string zbytekRadku(std::istringstream& ss) {
    std::string zbytek;
    std::getline(ss, zbytek);
    size_t start = zbytek.find_first_not_of(' ');
    return (start == std::string::npos) ? "" : zbytek.substr(start);
}

// Načte číselný argument do prikaz.id; při chybě nastaví Neznamy a vrátí false.
inline bool nactiIdArgument(std::istringstream& ss, Prikaz& prikaz) {
    std::string idStr;
    if (!(ss >> idStr)) {
        prikaz.typ = TypPrikazu::Neznamy;
        return false;
    }
    try {
        prikaz.id = std::stoi(idStr);
        return true;
    } catch (...) {
        prikaz.typ = TypPrikazu::Neznamy;
        return false;
    }
}

inline Prikaz rozeberPrikaz(const std::string& radek) {
    Prikaz prikaz;
    std::istringstream ss(radek);
    std::string token;
    ss >> token;

    if (token == "q") {
        prikaz.typ = TypPrikazu::Konec;
    } else if (token == "s") {
        prikaz.typ = TypPrikazu::Ulozit;
    } else if (token == "h") {
        prikaz.typ = TypPrikazu::Napoveda;
    } else if (token == "p") {
        prikaz.typ = TypPrikazu::Pridat;
        prikaz.popis = zbytekRadku(ss);
    } else if (token == "o" || token == "r") {
        prikaz.typ = (token == "o") ? TypPrikazu::Oznacit : TypPrikazu::Odebrat;
        nactiIdArgument(ss, prikaz);
    } else if (token == "n") {
        prikaz.typ = TypPrikazu::NovySeznam;
        prikaz.popis = zbytekRadku(ss);
    } else if (token == "v") {
        prikaz.typ = TypPrikazu::VybratSeznam;
        nactiIdArgument(ss, prikaz);
    } else if (token == "j") {
        prikaz.typ = TypPrikazu::PrejmenovatSeznam;
        if (nactiIdArgument(ss, prikaz)) {
            prikaz.popis = zbytekRadku(ss);
        }
    } else if (token == "d") {
        prikaz.typ = TypPrikazu::SmazatSeznam;
        std::string idStr;
        if (ss >> idStr) {
            try {
                prikaz.id = std::stoi(idStr);
            } catch (...) {
                prikaz.typ = TypPrikazu::Neznamy;
            }
            // ID seznamů začínají od 1; "d -1" nesmí kolidovat se sentinelem
            // pro "bez argumentu" a smazat aktivní seznam.
            if (prikaz.typ == TypPrikazu::SmazatSeznam && prikaz.id < 1) {
                prikaz.typ = TypPrikazu::Neznamy;
            }
        }
        // bez argumentu: id zůstává -1 = smazat aktivní seznam
    } else {
        prikaz.typ = TypPrikazu::Neznamy;
    }

    return prikaz;
}

inline void vytiskniNapovedu(std::ostream& out) {
    out << "TODOLIST(1)                        Napoveda\n"
           "\n"
           "PRIKAZY UKOLU\n"
           "  p <popis>        Prida novy ukol do aktivniho seznamu.\n"
           "  o <id>           Oznaci ukol jako hotovy.\n"
           "  r <id>           Odebere ukol ze seznamu.\n"
           "\n"
           "PRIKAZY SEZNAMU\n"
           "  n <nazev>        Zalozi novy seznam a prepne na nej.\n"
           "  v <id>           Prepne na seznam podle ID.\n"
           "  j <id> <nazev>   Prejmenuje seznam.\n"
           "  d                Smaze aktivni seznam (bez potvrzeni!).\n"
           "  d <id>           Smaze seznam podle ID.\n"
           "\n"
           "OSTATNI\n"
           "  s                Ulozi vsechny seznamy.\n"
           "  q                Ulozi a ukonci program.\n"
           "  h                Zobrazi tuto napovedu.\n"
           "\n"
           "Pokracuj stiskem Enteru...\n";
}

inline void vytiskniSeznamy(std::ostream& out, const StavSeznamu& stav) {
    out << "Seznamy: ";
    for (size_t i = 0; i < stav.seznamy.size(); ++i) {
        const Seznam& seznam = stav.seznamy[i];
        if (i > 0) out << " | ";
        std::string polozka = "[" + std::to_string(seznam.id) + "] " + seznam.nazev
                              + " (" + formatujProcenta(seznam.ukoly) + ")";
        if (seznam.id == stav.aktivniId) {
            out << "\033[1m>" << polozka << "<\033[0m";
        } else {
            out << polozka;
        }
    }
    out << "\n";
}

inline void vykresliObrazovku(std::ostream& out,
                              const StavSeznamu& stav,
                              const std::string& zprava) {
    vytiskniSeznamy(out, stav);
    const Seznam* aktivni = najdiSeznam(stav.seznamy, stav.aktivniId);
    out << "=== " << aktivni->nazev << " ===\n";
    if (aktivni->ukoly.empty()) {
        out << "Zadne ukoly.\n";
    } else {
        vytiskniUkoly(out, aktivni->ukoly);
    }
    out << "\n";
    if (!zprava.empty()) {
        out << zprava << "\n\n";
    }
    out << "Prikazy: p <popis> | o <id> | r <id> | n <nazev> | v <id> | j <id> <nazev> | d [id] | s | q | h\n"
        << "> ";
}
