# FLP projekt 2: Turingův stroj

> Ondřej Ondryáš
> xondry02@stud.fit.vutbr.cz
> 2022/2023

Program je možné přeložit s použitím dodaného Makefile. Alternativně je možné jej načíst do swipl a „spustit“ rezolucí predikátu `start`.

## Vstup

Vstup programu odpovídá zadání. Oproti zadání je umožněno, aby byly stavy automatu označen více než jedním písmenem, první písmeno však vždy musí být velké. Příklad validního přechodu na vstupu: `S a Q1x b`. 

Prázdný symbol je reprezentován znakem mezery, je však nutné dodržet oddělení mezerami z obou stran. Ve všech případech musí být hned za posledním znakem pravidla (nový symbol nebo akce L/R) znak nového řádku, jiné bílé ani jiné znaky nejsou tolerovány! Neplatný vstup povede na selhání predikátu `read_input`, a tedy i `start`.

Pravidlo, které na stavu S mění `a` na prázdný symbol je zapsáno jako (symbol `_` zde označuje mezeru, sekvence `\n` nový řádek): `S_a_S__\n`, opačná změna by byla zapsána jako `S___S_a\n`.

Závěrečný řádek s počátečním obsahem pásky je od ostatních odlišen tak, že začíná malým písmenem, po jeho zpracování končí čtení vstupu (nehledě na další případný obsah).

## Princip výpočtu

Splnění predikátu `start` je podmíněno splněním predikátu `read_input` a `turing(Output)`. První z nich zajistí načtení vstupu a uložení pravidel a počátečního stavu pásky stroje do interní reprezentace. Druhý pak provádí simulaci stroje, a to vyhodnocením predikátu `tm_step`, který volí odpovídající pravidlo, vytváří modifikovanou reprezentaci pásky a předává ji sám sobě v rekurzivním volání.

Stroj samotný je navržen tak, že v jednom kroce vždy změní symbol a provede jednu z akcí: „pohyb doleva“, „pohyb doprava“ nebo „stůj“. Pravidla jsou reprezentována **dynamickými predikáty** `rule(Stav, Symbol, NovyStav, NovySymbol, Akce)`, všechny parametry se vážou na atomy. Použitý výpočetní model se tedy lehce odlišuje od toho v zadání, který v jednom kroce buď mění symbol, nebo provádí akci; nicméně formát vstupu zadání odpovídá, převod z modelu v zadání spočívá pouze ve správném způsobu vytvoření pravidel:
- pro přepisovací pravidlo na vstupu (např. `S a B c`) se vytvoří predikát s akcí „stůj“ (např. `rule(S, a, B, c, stay).`),
- pro posunovací pravidlo na vstupu (např. `S a B R`) se vytvoří predikát, který symbol přepíše sám na sebe (např. `rule(S, a, B, a, right).`).

Tento způsob vnitřní reprezentace stroje byl zvolen z důvodu jednoduchosti, kdy každý krok stroje konzistentně spočívá ze dvou kroků výpočtu: změny symbolu a „posunu hlavy“.

Páska je uvnitř reprezentována dvěma seznamy atomů: `L` a `R`. Každý atom reprezentuje symbol na pásce. Seznam `L` reprezentuje obsah pásky *před* hlavou stroje, ten je zde však uložen *v opačném pořadí* (hlavička seznamu odpovídá symbolu nejblíž k hlavě). Seznam `R` reprezentuje zbytek pásky, tedy první prvek nese symbol pod hlavou, další prvky nesou symboly napravo od ní. Posun hlavy, tedy akce `left` a `right`, v zásadě spočívá v přesunu atomů (symbolů) mezi těmito seznamy (částmi pásky), a případně generování nový prázdných symbolů – např. posun doleva při neprázdné levé části pásky znamená přesun hlavičky seznamu `L` na začátek seznamu `R`. Tyto posuny vyhodnocuje predikát `move_head`.

Predikát `tm_step` provede unifikaci aktuálního stavu `State` a aktuálního symbolu pod hlavou `Symbol` s predikáty `rule(State, Symbol, ...)`, čímž získá nový stav, symbol a akci. V případě, že by v budoucnu někdy tato unifikace neuspěla, se Prolog backtrackingem vrací právě k tomuto vyhodnocení a volí alternativní pravidlo.

Stejný predikát také skrz rekurzivní vyhodnocování propaguje a postupně tvoří seznam řetězců s textovými reprezentacemi jednotlivých konfigurací.

Vstup je načítán jako seznamy kódů znaků (historická verze prologu na merlinovi bohužel neobsahuje funkci `split_string`, takže s řetězcovými reprezentacemi se pracuje hůř), načtené seznamy jsou rozděleny podle požadovaného formátu a převedeny na atomy, ze kterých se utvoří dynamické predikáty a vloží se do databáze.

## Testy

- test1: příklad ze zadání
- test2: TS, který modifikuje pásku ve tvaru `a^n` na tvar `a^nb^n`
- test3: TS, který přijímá jazyk `a^nb^nc^n`
- test4: TS, který modifikuje pásku ve tvaru `a^n` na tvar `b^n`
- test5: TS, který modifikuje pásku ve tvaru `a^(2n)` na tvar `(ab)^n`

## Zdroje

Implementace Turingova stroje byla inspirována tímto zdrojem: https://www.swi-prolog.org/pack/file_details/turing/prolog/turing.pl.

Predikát `take_until/2` byl převzat z https://stackoverflow.com/a/26514232.

Kromě těchto zdrojů bylo čerpáno pouze z dokumentace SWI Prolog.

Testovací TS byly převzaty z přednášky TIN.
