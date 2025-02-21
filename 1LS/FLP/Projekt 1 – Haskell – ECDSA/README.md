# FLP projekt 1: ECDSA

Implementace řeší zadání v celém jeho rozsahu.

**Implementace využívá generátor pseudonáhodných čísel `StdGen` z balíku `random`. Ten není možné považovat za kryptograficky bezpečný – a zbytek implementace ostatně taky ne.**

## Rozhraní

Rozhraní programu odpovídá zadání. Program vyžaduje **právě jeden** z parametrů:
- `-i`: načte a vypíše parametry křivky
- `-k`: načte parametry křivky, vygeneruje pár klíčů
- `-s`: načte parametry křivky, klíč a hash zprávy, vygeneruje podpis
- `-v`: načte parametry křivky, veřejný klíč, podpis a podepsaný hash, ověří podpis

Druhým parametrem může být cesta ke vstupnímu souboru, pokud není uvedena, použije se standardní vstup. Větší nebo nulové množství argumentů je považováno za chybný vstup a program je ukončen.

V případě chybného vstupu je vypsána na standardní chybový výstup hláška a program končí s nenulovým návratovým kódem. 

## Implementační detaily

Program se korektně chová pro vstupy, které přesně odpovídají zadání. Je permisivnější zejména v použití mezer, které nejsou vyžadovány, případně je možné nahradit je tabulátory. 

Parametry křivky (a, b, g.x, g.y) mohou volitelně obsahovat znaménko a být záporné, u hexadecimálních hodnot je znaménko očekáváno před sekvencí `0x` (viz např. `test/curves/02_bn-2-254`). 

U hexadecimálních hodnot nezáleží na velikosti písmen. Není možné použít hodnoty v jiných soustavách než v takových, v jakých je uvádí zadání.

**Očekává se, že vstup je zakončen znakem nového řádku**, jak je zvykem na unixových systémech.

Na vstupu SEC formátu veřejného klíče se nekontroluje, zda délka klíče odpovídá specifikaci SEC – řetězec oktetů má mít délku `ceil((log2 q) / 8)`, kde `q` je parametr použité křivky (prvočíselný modulus). Výstup této specifikaci odpovídá.

## Testování

Pro testování jsem si napsal (nikterak pěkně) utilitku v C#, která pro různé křivky (zkoušel jsem na pěti přiložených v `test/curves`) generuje několik klíčů, s každým podepíše několik zpráv a poté zpětně otestuje, zda je výsledný podpis platný a také zda je podpis s pozměněnými bity neplatný.

Spuštění testů vyžaduje nainstalované .NET 7 SDK, poté stačí spustit `make test`, případně `dotnet run --project test/testprog` (z top-level adresáře). Jako první argument je možné předat cestu ke spustitelnému souboru, jinak se použije `stack run`. Jako druhý argument je možné pevně zvolit seed PRNG, který generuje vstupní „hashe“ a také náhodné chyby pro testy validace nesprávných podpisů.