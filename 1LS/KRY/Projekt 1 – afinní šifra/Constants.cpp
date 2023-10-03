/**
 * @file   Constants.cpp
 * @brief  A container module for the top 400 Czech words list set.
 * @author Ondřej Ondryáš <xondry02@stud.fit.vut.cz>
 * @date   2023-04
 */

#include "Constants.h"

const std::unordered_set<std::string> TOP_N_WORDS = {
        "BYT", "SE", "NA", "TEN", "ON", "ZE", "KTERY", "MIT", "DO", "JA",
        "JEHO", "ALE", "SVUJ", "JAKO", "ZA", "MOCI", "ROK", "PRO", "TAK", "PO", "TENTO", "CO", "KDYZ",
        "VSECHEN", "UZ", "JAK", "ABY", "OD", "NEBO", "RICI", "JEDEN", "JEN", "MUJ", "JENZ", "CLOVEK", "TY", "STAT",
        "MUSET", "VELKY", "CHTIT", "TAKE", "AZ", "NEZ", "JESTE", "PRI", "JIT", "PAK",
        "PRED", "DVA", "VSAK", "ANI", "VEDET", "NOVY", "HODNE", "PODLE", "DALSI", "CELY", "JINY", "PRVNI",
        "MEZI", "DAT", "TADY", "DEN", "TAM", "KDE", "DOBA", "KAZDY", "DRUHY", "MISTO", "DOBRY", "TAKOVY",
        "STRANA", "PROTOZE", "NIC", "ZACIT", "NECO", "ZIVOT", "VIDET", "RIKAT", "ZEME", "DITE", "MALY", "NE", "SAM",
        "BEZ", "RUKA", "CI", "SVET", "DOSTAT", "PRACE", "NEJAKY", "PROTO",
        "POD", "TRI", "KDY", "MESTO", "PRIJIT", "DOBRE", "ZENA", "MUZ", "TED", "KDO", "JIZ", "NAD", "ASI", "STARY",
        "NAPRIKLAD", "PRIPAD", "VYSOKY", "ZADNY", "SPOLECNOST", "NEKOLIK", "NEKTERY", "TEDY", "CESTA", "POKUD", "DELAT",
        "HLAVA", "CAS", "POSLEDNI", "OKO", "PRAVE", "TVUJ", "PRAHA", "VCC", "VODA", "CESKY", "PROTI", "VELMI", "JAKY",
        "PRES", "DUM", "DNES", "PAN", "CAST", "TREBA", "KDYBY", "BRZY", "LZE", "FIRMA", "OBA", "SLOVO", "TENHLE",
        "VLASTNI", "NIKDY", "KORUNA", "MOZNY", "CHVILE", "UDELAT", "POUZE", "PROC", "SYSTEM", "KONEC", "KOLEM", "MALO",
        "MYSLIT", "STEJNE", "MOC", "PROBLEM", "MILION", "NIKDO", "STALE", "TOTIZ", "NEKDO", "TISIC",
        "VUBEC", "ZAKON", "NECHAT", "NAJIT", "COZ", "CENA", "RAD", "SKOLA", "HLAVNI", "SKUPINA", "MLADY", "ZPUSOB",
        "CASTO", "RADA", "JEDINY", "MLUVIT", "VRATIT", "RUZNY", "HRAT", "SNAD", "ZUSTAT", "VZIT", "DLOUHY", "CEKAT",
        "PATRIT", "OBLAST", "VLADA", "DOKONCE", "ZNAMY", "DOKAZAT", "PENIZE", "MNOHO", "OTAZKA", "VEST", "PRAVO",
        "SPISE" "POZDE", "ZIT", "ZASE", "PREDEVSIM", "OVSEM", "TAKZE", "STOLETI", "PRECE", "MOZNOST", "OSTATNI",
        "CISLO", "JENOM", "DALE", "UVEST", "ZNOVU", "MOZNA", "SLUZBA", "JESTLI", "SITUACE", "STEJNY", "HODINY", "MESIC",
        "ZISKAT", "CTYRI", "JMENO", "DVERE", "DALEKO", "OTEC", "ZNAT", "ZMENA", "EVROPSKY", "VETSINA", "NAKONEC",
        "NYNI", "NOC", "SNAZIT", "SILA", "ZDE", "DOJIT", "POTOM", "ZNAMENAT", "VYPADAT", "PET", "INFORMACE",
        "AMERICKY", "TROCHU", "TYDEN", "DULEZITY", "PRILIS", "VZTAH", "TRH", "TELO", "VALKA", "HNED", "SLYSET", "TEMER",
        "DLOUHO", "RODINA", "ZATIM", "ROZHODNOUT", "VYSLEDEK", "ANO", "PRACOVAT", "STATNI", "POHLED", "ZAJEM", "ZEPTAT",
        "ULICE", "POTREBOVAT", "TVAR", "ZDAT", "KNIHA", "DUVOD", "HLAS", "FILM", "PRAVDA", "NEKDY",
        "OBJEVIT", "DOST", "ZAKLADNI", "ZCELA", "PRITOM", "PANI", "SMRT", "MATKA", "SICE", "RYCHLE", "CITIT", "MINUTA",
        "PAR", "PROCENTO", "PROGRAM", "URCITY", "VECER", "BEHEM", "EXISTOVAT", "KVULI", "HODNOTA", "BILY", "NOHA",
        "JISTY", "JEDNOU", "PODIVAT", "VCERA", "PLNY", "CLEN", "VLASTNE", "LIDSKY", "SEDET", "PODMINKA", "VZDY",
        "NAVIC", "OSOBA", "HRA", "POCET", "TEHDY", "REPUBLIKA", "PRIMO", "STAV", "BANKA", "POLITICKY", "HODINA",
        "OBRAZ", "ZEJMENA", "AT", "CINNOST", "PODNIK", "JINAK", "DRUH", "SOUD", "OPET", "NARODNI", "URAD", "PODOBNY",
        "DESET", "KROME", "NEJEN", "AUTO", "UKAZAT", "CERNY", "ZDA", "PLATIT", "SPATNY", "OTEVRIT", "VZDYCKY", "DAVAT",
        "CIL", "PSAT", "CHODIT", "JEDNOTLIVY", "SOCIALNI", "POVAZOVAT", "SOUCASNY", "EVROPA", "NEBOT", "JAN", "ZPRAVA",
        "TYP", "POLICIE", "STACIT", "OPRAVDU", "SKUTECNOST", "VERIT", "POCIT", "DILO", "SILNY", "STO", "SPOLU",
        "NEMECKY", "VEDLE", "TVRDIT", "ZAKLAD", "PORAD", "BUH", "PROSTREDI", "SAMOZREJME", "JET", "ZREJME", "ZJISTIT",
        "SVETLO", "SMYSL", "SMER", "RUST", "REDITEL", "RANO", "ZATIMCO", "MYSLET", "LASKA", "DIVADLO", "PROCES",
        "PODARIT", "FORMA", "STUL", "PRY", "POKRACOVAT", "NAZOR", "TEZKY", "OKNO", "POMOCI", "SVETOVY", "NAPSAT"
};