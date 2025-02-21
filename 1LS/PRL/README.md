# Paralelní a distribuované algoritmy
### Ak. rok 2022/2023. [Karta předmětu](https://www.fit.vut.cz/study/course/259440/.cs).

## Projekt 1 – Implementace algoritmu Parallel Splitting
Hodnocení: 10 / 10

> Pomocí knihovny OpenMPI implementujte v jazyce C++ úlohu Parallel Splitting tak, jak byla uvedena na přednášce PRL. Pouze v těch částech, kdy algoritmus předpokládá využití sdílené paměti, použijte vhodně prostředky MPI.
> Vstupem je posloupnost osmi náhodných čísel uložených v souboru. Soubor numbers obsahující čísla velikosti 1 byte, která jdou bez mezery za sebou. V případě tohoto projektu předpokládejte rozsah "numbers" od 8 do 64.
> Výsledné rozdělení vstupní posloupnosti bude vytištěno na obrazovku terminálu.
> Kořenový root proces (číslo 0 v hlavním komunikátoru) načte všechna čísla ze souboru. Dále vybere medián v polovině posloupnosti (v případě sudého počtu prvků bude volen blíže počátku posloupnosti), rozešle jej ostatním procesům a také jim přidělí jejich část posloupnosti. Rozdělení na části je rovnoměrné. Předpokládejte, že počet prvků v posloupnosti je vždy dělitelný beze zbytku počtem procesů, které jsou spuštěny. K rozeslání použijte kolektivní operaci `MPI_Scatter`. Po provedení patřičné části práce procesy použitím kolektivní operace `MPI_Gatherv` odevzdají výsledky kořenovému procesu, který na závěr celkový výsledek vytiskne na obrazovku. Tedy pro sestavení výsledků není možné použít sdílenou paměť. Namísto toho použijte, jak již bylo uvedeno, kolektivní operac `MPI_Gatherv`.

## Projekt 2 – Implementace paralelního algoritmu k-means
Hodnocení: 10 / 10

> Pomocí knihovny OpenMPI implementujte v jazyce C++ algoritmus K-Means pracující se skalárními (jednorozměrnými) hodnotami. Algoritmus paralelizujte a navrhněte tak, aby měl co nejmenší časovou složitost. Pro posloupnost N hodnot budete mít k dispozici N procesorů. Dále předpokládajte, že algoritmus vytváří 4 shluky (tedy konkrétně se bude jednat o 4-means) a vstupní data budou obsahovat minimálně 4 a maximálně 32 hodnot. V dokumentaci představte vaše řešení a analyticky uveďte jeho časovou složitost.
> K dispozici máte C++ (přeložitelné pomocí mpic++ kvůli jednotnému skriptu test.sh, ne nutně objektově), OpenMPI a nic jiného.
> Vstupem je posloupnost čtyř až třiceti dvou náhodných čísel uložených v souboru. Vytvořte skript, který bude obdobný tomu, který jste vytvářeli v prvním projektu. Jako výstup proběhne vytištění jednotlových shluků vhodným způsobem.
> Počáteční hodnoty středů shluků volte z prvních čtyřech prvků posloupnosti. Redukovat v MPI můžete i pole hodnot, dejme tomu o čtyřech prvcích, to by mohlo být užitečné. Pokud po spuštění počet procesů N se nerovná velikosti vstupní posloupnosti, pak v případě že je na vstupu hodnot vice, pracujte s prvními N hodnotami. Pokud je hodnot méně, výpočet neprovádějte a nechte aplikaci toto oznámit na obrazovku.
