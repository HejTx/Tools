#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <limits>

#include "sifrovani.hpp"

struct Task {
    int id;
    std::string description;
    bool done = false;
    std::string termin;   // dd/mm/yy, prázdný = bez termínu
    int priorita = 2;     // 1 vysoká, 2 normální, 3 nízká
};

// Formát dd/mm/yy: číslice + lomítka, den 01-31, měsíc 01-12.
inline bool jePlatnyTermin(const std::string& termin) {
    if (termin.size() != 8 || termin[2] != '/' || termin[5] != '/') return false;
    for (size_t i : {0, 1, 3, 4, 6, 7}) {
        if (!std::isdigit(static_cast<unsigned char>(termin[i]))) return false;
    }
    int den = std::stoi(termin.substr(0, 2));
    int mesic = std::stoi(termin.substr(3, 2));
    return den >= 1 && den <= 31 && mesic >= 1 && mesic <= 12;
}

// Porovnávací klíč rr*10000+mm*100+dd; bez termínu (nebo poškozený,
// např. z ručně upraveného souboru) = max, řadí se nakonec.
inline int klicTerminu(const std::string& termin) {
    if (!jePlatnyTermin(termin)) return std::numeric_limits<int>::max();
    int den = std::stoi(termin.substr(0, 2));
    int mesic = std::stoi(termin.substr(3, 2));
    int rok = std::stoi(termin.substr(6, 2));
    return rok * 10000 + mesic * 100 + den;
}

// Chybová hláška pro neplatný název seznamu, jinak "".
inline std::string zkontrolujNazevSeznamu(const std::string& nazev) {
    if (nazev.empty()) return "Nazev nesmi byt prazdny.";
    if (nazev.find(';') != std::string::npos) return "Nazev nesmi obsahovat znak ';'.";
    if (nazev.find('/') != std::string::npos) return "Nazev nesmi obsahovat znak '/'.";
    if (nazev[0] == '.') return "Nazev nesmi zacinat teckou.";
    if (nazev.size() >= 7 && nazev.substr(nazev.size() - 7) == ".hotove") {
        return "Nazev nesmi koncit na '.hotove'.";  // koliduje s archivnimi soubory
    }
    return "";
}

// Globální nešifrované předvolby (razeni, naposledy otevřený seznam).
struct Nastaveni {
    int razeni = 1;
    std::string posledni;
};

inline std::string serializujNastaveni(const Nastaveni& nastaveni) {
    return "razeni;" + std::to_string(nastaveni.razeni)
         + "\nposledni;" + nastaveni.posledni + "\n";
}

inline Nastaveni parsujNastaveni(const std::string& obsah) {
    Nastaveni nastaveni;
    std::istringstream in(obsah);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("razeni;", 0) == 0) {
            try {
                nastaveni.razeni = std::stoi(line.substr(std::string("razeni;").size()));
            } catch (...) {}
        } else if (line.rfind("posledni;", 0) == 0) {
            nastaveni.posledni = line.substr(std::string("posledni;").size());
        }
    }
    if (nastaveni.razeni != 2) nastaveni.razeni = 1;
    return nastaveni;
}

struct VerzeZaznam {
    std::string hash;
    std::string datum;
};

// Sestaví git příkaz nad datovým adresářem (adresář v uvozovkách).
inline std::string sestavGitPrikaz(const std::string& adresar, const std::string& argumenty) {
    return "git -C \"" + adresar + "\" " + argumenty;
}

// Rozparsuje výstup `git log --format=%h;%ad` (řádek = hash;datum).
inline std::vector<VerzeZaznam> parsujVerze(const std::string& vystup) {
    std::vector<VerzeZaznam> verze;
    std::istringstream in(vystup);
    std::string line;
    while (std::getline(in, line)) {
        size_t oddelovac = line.find(';');
        if (oddelovac == std::string::npos || oddelovac == 0 || oddelovac + 1 >= line.size()) {
            continue;
        }
        verze.push_back({line.substr(0, oddelovac), line.substr(oddelovac + 1)});
    }
    return verze;
}

// Modální stránka verzí seznamu (vzor: vytiskniNapovedu).
inline void vytiskniVerze(std::ostream& out, const std::string& nazev,
                          const std::vector<VerzeZaznam>& verze) {
    out << "=== Verze: " << nazev << " ===\n";
    if (verze.empty()) {
        out << "Zadna ulozena verze.\n";
    } else {
        for (size_t i = 0; i < verze.size(); ++i) {
            out << "[" << (i + 1) << "] " << verze[i].datum << "\n";
        }
        out << "\n"
            << "Obnovis prikazem vz <cislo>.\n";
    }
    out << "\n"
        << "Pokracuj stiskem Enteru...\n";
}

// Podíl hotových úkolů jako "50.0%"; prázdno = "0.0%".
inline std::string formatujProcenta(int hotovo, int celkem) {
    double procenta = (celkem == 0) ? 0.0 : 100.0 * hotovo / celkem;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.1f%%", procenta);
    return buf;
}

inline std::string formatujProcenta(const std::vector<Task>& ukoly) {
    int hotovo = 0;
    for (const auto& ukol : ukoly) {
        if (ukol.done) ++hotovo;
    }
    return formatujProcenta(hotovo, static_cast<int>(ukoly.size()));
}

inline std::string serializujUkoly(const std::vector<Task>& ukoly) {
    std::ostringstream out;
    for (const auto& ukol : ukoly) {
        out << ukol.id << ";" << ukol.description << ";" << ukol.done << ";" << ukol.termin
            << ";" << ukol.priorita << "\n";
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
        std::string idStr, description, doneStr, terminStr, prioritaStr;

        std::getline(ss, idStr, ';');
        std::getline(ss, description, ';');
        std::getline(ss, doneStr, ';');
        std::getline(ss, terminStr, ';');    // starý 3-polový řádek -> prázdný termín
        std::getline(ss, prioritaStr, ';');  // starý 4-polový řádek -> priorita 2

        Task ukol;
        try {
            ukol.id = std::stoi(idStr);
        } catch (...) {
            continue;  // poškozený řádek se přeskočí, ať migrace nespadne
        }
        ukol.description = description;
        ukol.done = (doneStr == "1");
        ukol.termin = terminStr;
        try {
            ukol.priorita = std::stoi(prioritaStr);
        } catch (...) {
            ukol.priorita = 2;
        }
        if (ukol.priorita < 1 || ukol.priorita > 3) ukol.priorita = 2;

        ukoly.push_back(ukol);
    }

    return ukoly;
}

struct Seznam {
    int id;
    std::string nazev;
    std::vector<Task> ukoly;                                    // jen odemčené
    std::vector<Task> archiv;   // hotové přesunuté příkazem c (jen odemčené)
    bool odemceno = true;   // pahýly (jen název) vytváří pouze startovní sken
    std::vector<unsigned char> klic;                            // jen odemčené
    std::array<unsigned char, crypto_pwhash_SALTBYTES> sul{};   // jen odemčené
};

struct StavSeznamu {
    std::vector<Seznam> seznamy;
    int aktivniId = 1;
    int razeni = 1;  // 1 = podle ID, 2 = podle termínu
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
    out << "@razeni;" << stav.razeni << "\n";
    for (const auto& seznam : stav.seznamy) {
        out << "#seznam;" << seznam.id << ";" << seznam.nazev
            << (seznam.odemceno ? "" : ";zamceno") << "\n"
            << serializujUkoly(seznam.ukoly);
        for (const auto& ukol : seznam.archiv) {
            out << "~" << ukol.id << ";" << ukol.description << ";" << ukol.done
                << ";" << ukol.termin << ";" << ukol.priorita << "\n";
        }
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
        if (!line.empty() && line[0] == '~') continue;  // archivní řádky (jen pro porovnání)
        if (line.rfind("@razeni;", 0) == 0) {
            try {
                stav.razeni = std::stoi(line.substr(std::string("@razeni;").size()));
            } catch (...) {}
            continue;
        }
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
    if (stav.aktivniId != 0 && najdiSeznam(stav.seznamy, stav.aktivniId) == nullptr) {
        stav.aktivniId = stav.seznamy.front().id;
    }
    if (stav.razeni != 2) stav.razeni = 1;
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
    if (id != 0 && najdiSeznam(stav.seznamy, id) == nullptr) return false;  // 0 = přehled Vše
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

    if (stav.aktivniId != 0 && najdiSeznam(stav.seznamy, stav.aktivniId) == nullptr) {
        stav.aktivniId = 0;  // žádný odemčený -> přehled
        for (const auto& seznam : stav.seznamy) {
            if (seznam.odemceno) {
                stav.aktivniId = seznam.id;
                break;
            }
        }
    }
    return true;
}

// Kopie úkolů v zobrazovacím pořadí (1 = pořadí dat/ID, 2 = podle termínu).
inline std::vector<Task> serazeneUkoly(const std::vector<Task>& ukoly, int razeni) {
    std::vector<Task> vysledek = ukoly;
    if (razeni == 2) {
        std::stable_sort(vysledek.begin(), vysledek.end(), [](const Task& a, const Task& b) {
            if (a.done != b.done) return !a.done;  // hotove nakonec
            if (klicTerminu(a.termin) != klicTerminu(b.termin)) {
                return klicTerminu(a.termin) < klicTerminu(b.termin);
            }
            if (a.priorita != b.priorita) return a.priorita < b.priorita;
            return a.id < b.id;
        });
    }
    return vysledek;
}

struct PolozkaPrehledu {
    int seznamId;
    Task ukol;
};

// Všechny úkoly napříč seznamy v zobrazovacím pořadí přehledu „Vše".
inline std::vector<PolozkaPrehledu> sestavPrehled(const StavSeznamu& stav) {
    std::vector<PolozkaPrehledu> polozky;
    for (const auto& seznam : stav.seznamy) {
        if (!seznam.odemceno) continue;
        for (const auto& ukol : seznam.ukoly) {
            polozky.push_back({seznam.id, ukol});
        }
    }
    if (stav.razeni == 2) {
        std::stable_sort(polozky.begin(), polozky.end(),
                         [](const PolozkaPrehledu& a, const PolozkaPrehledu& b) {
            if (a.ukol.done != b.ukol.done) return !a.ukol.done;  // hotove nakonec
            if (klicTerminu(a.ukol.termin) != klicTerminu(b.ukol.termin)) {
                return klicTerminu(a.ukol.termin) < klicTerminu(b.ukol.termin);
            }
            if (a.ukol.priorita != b.ukol.priorita) return a.ukol.priorita < b.ukol.priorita;
            if (a.seznamId != b.seznamId) return a.seznamId < b.seznamId;
            return a.ukol.id < b.ukol.id;
        });
    }
    return polozky;
}

// Přesune úkol ze zdrojového seznamu do cílového; v cíli dostane nové ID.
// 0 = OK, 1 = úkol (či zdroj) nenalezen, 2 = cíl neexistuje, 3 = cíl == zdroj.
inline int presunUkol(StavSeznamu& stav, int zdrojId, int ukolId, int cilId) {
    Seznam* zdroj = najdiSeznam(stav.seznamy, zdrojId);
    if (!zdroj) return 1;
    auto it = std::find_if(zdroj->ukoly.begin(), zdroj->ukoly.end(),
                           [ukolId](const Task& ukol) { return ukol.id == ukolId; });
    if (it == zdroj->ukoly.end()) return 1;
    if (cilId == zdrojId) return 3;
    Seznam* cil = najdiSeznam(stav.seznamy, cilId);
    if (!cil) return 2;
    Task presouvany = *it;
    zdroj->ukoly.erase(it);
    presouvany.id = cil->ukoly.empty() ? 1 : cil->ukoly.back().id + 1;
    cil->ukoly.push_back(presouvany);
    return 0;
}

// Zašifruje a atomicky zapíše zadané úkoly daným klíčem a solí (temp+rename).
// Vrací false, když se zápis nezdařil (původní soubor zůstává nedotčen).
inline bool ulozUkolyDoSouboru(const std::vector<Task>& ukoly,
                               const std::vector<unsigned char>& klic,
                               const std::array<unsigned char, crypto_pwhash_SALTBYTES>& sul,
                               const std::string& cesta) {
    const std::string docasny = cesta + ".tmp";

    std::ofstream out(docasny, std::ios::binary);
    if (!out) {
        std::cerr << "Nepodarilo se otevrit soubor pro zapis: " << docasny << "\n";
        return false;
    }

    out << zasifruj(serializujUkoly(ukoly), klic, sul);
    out.flush();
    if (!out) {
        std::cerr << "Zapis do souboru " << docasny << " se nezdaril, puvodni data zustavaji.\n";
        out.close();
        std::remove(docasny.c_str());
        return false;
    }
    out.close();

    if (std::rename(docasny.c_str(), cesta.c_str()) != 0) {
        std::cerr << "Nepodarilo se prejmenovat " << docasny << " na " << cesta << ".\n";
        std::remove(docasny.c_str());
        return false;
    }
    return true;
}

inline bool ulozSeznamDoSouboru(const Seznam& seznam, const std::string& cesta) {
    return ulozUkolyDoSouboru(seznam.ukoly, seznam.klic, seznam.sul, cesta);
}

// Přečte celý soubor jako syrové bajty. nullopt = soubor nejde otevřít.
inline std::optional<std::string> nactiObsahSouboru(const std::string& soubor) {
    std::ifstream in(soubor, std::ios::binary);
    if (!in) return std::nullopt;

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// dnes = dnešní klíč (formát klicTerminu); 0 = kontrola prošlých vypnutá.
inline void vytiskniUkol(std::ostream& out, const Task& ukol,
                         const std::string& prefixId = "", int dnes = 0) {
    bool prosly = !ukol.done && !ukol.termin.empty()
                  && dnes != 0 && klicTerminu(ukol.termin) < dnes;
    bool vysoka = !ukol.done && !prosly && ukol.priorita == 1;
    if (ukol.done) out << "\033[90m";
    else if (prosly) out << "\033[31m";
    else if (vysoka) out << "\033[33m";
    out << "ID: " << prefixId << ukol.id << ", Popis: " << ukol.description
        << ", Dokonceno: " << (ukol.done ? "Ano" : "Ne");
    if (!ukol.termin.empty()) out << ", Termin: " << ukol.termin;
    if (ukol.priorita == 1) out << ", Priorita: vysoka";
    else if (ukol.priorita == 3) out << ", Priorita: nizka";
    if (ukol.done || prosly || vysoka) out << "\033[0m";
    out << "\n";
}

inline void vytiskniUkoly(std::ostream& out, const std::vector<Task>& ukoly, int dnes = 0) {
    for (const auto& ukol : ukoly) {
        vytiskniUkol(out, ukol, "", dnes);
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

// Přesune hotové úkoly do archivu (dostanou nová archivní ID); vrací počet.
inline int archivujHotove(Seznam& seznam) {
    int pocet = 0;
    std::vector<Task> zbyle;
    for (auto& ukol : seznam.ukoly) {
        if (ukol.done) {
            ukol.id = seznam.archiv.empty() ? 1 : seznam.archiv.back().id + 1;
            seznam.archiv.push_back(ukol);
            ++pocet;
        } else {
            zbyle.push_back(ukol);
        }
    }
    seznam.ukoly = std::move(zbyle);
    return pocet;
}

// Vrátí úkol z archivu mezi aktivní (nové ID, done = false). false = nenalezen.
inline bool obnovUkol(Seznam& seznam, int archivId) {
    auto it = std::find_if(seznam.archiv.begin(), seznam.archiv.end(),
                           [archivId](const Task& ukol) { return ukol.id == archivId; });
    if (it == seznam.archiv.end()) return false;
    Task obnoveny = *it;
    seznam.archiv.erase(it);
    obnoveny.id = seznam.ukoly.empty() ? 1 : seznam.ukoly.back().id + 1;
    obnoveny.done = false;
    seznam.ukoly.push_back(obnoveny);
    return true;
}

// Nastaví (či prázdným řetězcem smaže) termín úkolu.
inline bool nastavTermin(std::vector<Task>& ukoly, int id, const std::string& termin) {
    for (auto& ukol : ukoly) {
        if (ukol.id == id) {
            ukol.termin = termin;
            return true;
        }
    }
    return false;
}

// Nastaví prioritu úkolu (1 vysoká, 2 normální, 3 nízká).
inline bool nastavPrioritu(std::vector<Task>& ukoly, int id, int priorita) {
    for (auto& ukol : ukoly) {
        if (ukol.id == id) {
            ukol.priorita = priorita;
            return true;
        }
    }
    return false;
}

// Změní jen popis; ID a done zůstávají.
inline bool upravitUkol(std::vector<Task>& ukoly, int id, const std::string& popis) {
    for (auto& ukol : ukoly) {
        if (ukol.id == id) {
            ukol.description = popis;
            return true;
        }
    }
    return false;
}

enum class TypPrikazu { Pridat, Oznacit, Odebrat, Konec, Neznamy, Ulozit,
                        NovySeznam, VybratSeznam, PrejmenovatSeznam, SmazatSeznam,
                        Napoveda, UpravitUkol, PresunoutUkol, VycistitHotove,
                        Zpet, ZmenaHesla, Termin, PrepnoutRazeni, Priorita,
                        Archiv, Obnovit, Verze };

struct Prikaz {
    TypPrikazu typ = TypPrikazu::Neznamy;
    std::string popis;
    int id = -1;
    int id2 = -1;
    int seznamUkolu = -1;  // seznam ze složeného ID <seznam>.<ukol>, -1 = aktivní
};

// Zbytek řádku bez úvodních mezer (popis úkolu / název seznamu).
inline std::string zbytekRadku(std::istringstream& ss) {
    std::string zbytek;
    std::getline(ss, zbytek);
    size_t start = zbytek.find_first_not_of(' ');
    return (start == std::string::npos) ? "" : zbytek.substr(start);
}

// Načte číselný argument do cíle; při chybě nastaví Neznamy a vrátí false.
inline bool nactiIdArgument(std::istringstream& ss, Prikaz& prikaz, int& cil) {
    std::string idStr;
    if (!(ss >> idStr)) {
        prikaz.typ = TypPrikazu::Neznamy;
        return false;
    }
    try {
        cil = std::stoi(idStr);
        return true;
    } catch (...) {
        prikaz.typ = TypPrikazu::Neznamy;
        return false;
    }
}

// Načte ID úkolu, volitelně složené <seznam>.<ukol> (seznam -> prikaz.seznamUkolu).
inline bool nactiUkolId(std::istringstream& ss, Prikaz& prikaz) {
    std::string token;
    if (!(ss >> token)) {
        prikaz.typ = TypPrikazu::Neznamy;
        return false;
    }
    size_t tecka = token.find('.');
    try {
        if (tecka != std::string::npos) {
            if (tecka == 0 || tecka + 1 >= token.size()) throw std::invalid_argument("id");
            prikaz.seznamUkolu = std::stoi(token.substr(0, tecka));
            prikaz.id = std::stoi(token.substr(tecka + 1));
        } else {
            prikaz.id = std::stoi(token);
        }
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
        nactiUkolId(ss, prikaz);
    } else if (token == "e") {
        prikaz.typ = TypPrikazu::UpravitUkol;
        if (nactiUkolId(ss, prikaz)) {
            prikaz.popis = zbytekRadku(ss);
        }
    } else if (token == "t") {
        prikaz.typ = TypPrikazu::Termin;
        if (nactiUkolId(ss, prikaz)) {
            prikaz.popis = zbytekRadku(ss);
        }
    } else if (token == "pr") {
        prikaz.typ = TypPrikazu::Priorita;
        if (nactiUkolId(ss, prikaz)) {
            prikaz.popis = zbytekRadku(ss);
        }
    } else if (token == "z") {
        prikaz.typ = TypPrikazu::PrepnoutRazeni;
    } else if (token == "m") {
        prikaz.typ = TypPrikazu::PresunoutUkol;
        if (nactiUkolId(ss, prikaz)) {
            nactiIdArgument(ss, prikaz, prikaz.id2);
        }
    } else if (token == "c") {
        prikaz.typ = TypPrikazu::VycistitHotove;
    } else if (token == "a") {
        prikaz.typ = TypPrikazu::Archiv;
    } else if (token == "ob") {
        prikaz.typ = TypPrikazu::Obnovit;
        nactiIdArgument(ss, prikaz, prikaz.id);
    } else if (token == "u") {
        prikaz.typ = TypPrikazu::Zpet;
    } else if (token == "zh") {
        prikaz.typ = TypPrikazu::ZmenaHesla;
    } else if (token == "n") {
        prikaz.typ = TypPrikazu::NovySeznam;
        prikaz.popis = zbytekRadku(ss);
    } else if (token == "vz") {
        prikaz.typ = TypPrikazu::Verze;
        std::string idStr;
        if (ss >> idStr) {
            try {
                prikaz.id = std::stoi(idStr);
            } catch (...) {
                prikaz.typ = TypPrikazu::Neznamy;
            }
            if (prikaz.typ == TypPrikazu::Verze && prikaz.id < 1) {
                prikaz.typ = TypPrikazu::Neznamy;
            }
        }
        // bez cisla: id zustava -1 = vypis verzi
    } else if (token == "v") {
        prikaz.typ = TypPrikazu::VybratSeznam;
        nactiIdArgument(ss, prikaz, prikaz.id);
    } else if (token == "j") {
        prikaz.typ = TypPrikazu::PrejmenovatSeznam;
        if (nactiIdArgument(ss, prikaz, prikaz.id)) {
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
           "  e <id> <popis>   Upravi popis ukolu.\n"
           "  t <id> <datum>   Nastavi termin (dd/mm/yy); t <id> bez data termin smaze.\n"
           "  pr <id> <1-3>    Nastavi prioritu (1 vysoka, 2 normalni, 3 nizka).\n"
           "  m <id> <sid>     Presune ukol do seznamu <sid>.\n"
           "  c                Presune hotove ukoly do archivu (v prehledu 0 vsude).\n"
           "  a                Zobrazi archiv hotovych ukolu aktivniho seznamu.\n"
           "  ob <id>          Obnovi ukol z archivu (vrati se jako nehotovy).\n"
           "\n"
           "PRIKAZY SEZNAMU\n"
           "  n <nazev>        Zalozi novy seznam a prepne na nej.\n"
           "  v <id>           Prepne na seznam podle ID; v 0 = prehled vsech ukolu.\n"
           "  j <id> <nazev>   Prejmenuje seznam.\n"
           "  d                Smaze aktivni seznam (bez potvrzeni!).\n"
           "  d <id>           Smaze seznam podle ID.\n"
           "\n"
           "OSTATNI\n"
           "  u                Vrati posledni zmenu (u znovu = zpet).\n"
           "  z                Prepina razeni: podle ID / podle terminu.\n"
           "  vz [n]           Verze aktivniho seznamu; vz <n> obnovi verzi n.\n"
           "  s                Ulozi vsechny seznamy.\n"
           "  q                Ulozi a ukonci program.\n"
           "  zh               Zmeni heslo souboru.\n"
           "  h                Zobrazi tuto napovedu.\n"
           "\n"
           "Ukoly lze adresovat i jako <seznam>.<ukol> (napr. o 2.3) - v prehledu 0 je to nutne.\n"
           "\n"
           "Pokracuj stiskem Enteru...\n";
}

// Vypíše řádek seznamů; při přesahu šířky zalamuje s odsazením pod
// "Seznamy: ". Viditelná délka se počítá bez ANSI kódů (v bajtech, ASCII).
// Modální stránka archivu aktivního seznamu (vzor: vytiskniNapovedu).
inline void vytiskniArchiv(std::ostream& out, const Seznam& seznam, int dnes = 0) {
    out << "=== Archiv: " << seznam.nazev << " ===\n";
    if (seznam.archiv.empty()) {
        out << "Zadny archivovany ukol.\n";
    } else {
        for (const auto& ukol : seznam.archiv) {
            vytiskniUkol(out, ukol, "", dnes);
        }
    }
    out << "\n"
        << "Pokracuj stiskem Enteru...\n";
}

inline void vytiskniSeznamy(std::ostream& out, const StavSeznamu& stav, int sirka = 80) {
    int hotovoCelkem = 0;
    int celkem = 0;
    for (const auto& seznam : stav.seznamy) {
        if (!seznam.odemceno) continue;
        celkem += static_cast<int>(seznam.ukoly.size());
        for (const auto& ukol : seznam.ukoly) {
            if (ukol.done) ++hotovoCelkem;
        }
    }
    std::vector<std::pair<int, std::string>> polozky;
    polozky.push_back({0, "[0] Vse (" + formatujProcenta(hotovoCelkem, celkem) + ")"});
    for (const auto& seznam : stav.seznamy) {
        polozky.push_back({seznam.id,
            seznam.odemceno
                ? "[" + std::to_string(seznam.id) + "] " + seznam.nazev
                      + " (" + formatujProcenta(seznam.ukoly) + ")"
                : "[" + std::to_string(seznam.id) + "] " + seznam.nazev + " (zamceno)"});
    }

    out << "Seznamy: ";
    int delkaRadku = 9;
    bool prvniNaRadku = true;
    for (const auto& [id, polozka] : polozky) {
        bool aktivni = (id == stav.aktivniId);
        int viditelna = static_cast<int>(polozka.size()) + (aktivni ? 2 : 0);
        if (!prvniNaRadku && delkaRadku + 3 + viditelna > sirka) {
            out << "\n         ";
            delkaRadku = 9;
            prvniNaRadku = true;
        }
        if (!prvniNaRadku) {
            out << " | ";
            delkaRadku += 3;
        }
        if (aktivni) {
            out << "\033[1m>" << polozka << "<\033[0m";
        } else {
            out << polozka;
        }
        delkaRadku += viditelna;
        prvniNaRadku = false;
    }
    out << "\n";
}

inline void vykresliObrazovku(std::ostream& out,
                              const StavSeznamu& stav,
                              const std::string& zprava,
                              int sirka = 80, int dnes = 0) {
    vytiskniSeznamy(out, stav, sirka);
    const char* rezim = (stav.razeni == 2) ? "termin" : "ID";
    if (stav.aktivniId == 0) {
        out << "=== Vse === (razeni: " << rezim << ")\n";
        std::vector<PolozkaPrehledu> polozky = sestavPrehled(stav);
        if (polozky.empty()) {
            out << "Zadne ukoly.\n";
        } else {
            for (const auto& polozka : polozky) {
                vytiskniUkol(out, polozka.ukol, std::to_string(polozka.seznamId) + ".", dnes);
            }
        }
    } else {
        const Seznam* aktivni = najdiSeznam(stav.seznamy, stav.aktivniId);
        out << "=== " << aktivni->nazev << " === (razeni: " << rezim << ")\n";
        std::vector<Task> ukoly = serazeneUkoly(aktivni->ukoly, stav.razeni);
        if (ukoly.empty()) {
            out << "Zadne ukoly.\n";
        } else {
            vytiskniUkoly(out, ukoly, dnes);
        }
    }
    out << "\n";
    if (!zprava.empty()) {
        out << zprava << "\n\n";
    }
    out << "\033[90mukol: p pridat · o hotovo · r odebrat · e upravit\n"
           "      m presunout · t termin · pr priorita · c uklidit · a archiv · ob obnovit\n"
           "seznam: n novy · v vybrat · j prejmenovat · d smazat\n"
           "jine: u zpet · z razeni · s ulozit · zh heslo · q konec · h napoveda · vz verze\033[0m\n"
        << "> ";
}
