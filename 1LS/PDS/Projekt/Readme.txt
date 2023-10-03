Nástroj vyžaduje:
- Python 3.10
- knihovny bencoder.pyx a scapy

Doporučeným postupem pro spuštění je vytvoření virtuálního prostředí pomocí venv nebo virtualenv:

venv ./venv   nebo   virtualenv ./venv
source ./venv/bin/activate
pip install -r requirements.txt
python bt-monitor.py

----

Dostupné jsou tyto přepínače:

-pcap <file>: povinný, cesta ke vstupnímu Pcapng souboru (Pcap by měly také fungovat)

-init: vypíše bootstrap uzly ve formátu ip/port/node ID/dom. jméno z DNS požadavku/dom. jméno z reverzního vyhledávání
       > poslední položka je vyplněna, pouze pokud je použit parametr -bh
       > parametr -bootstrap-cutoff nastavuje maximální počet UDP paketů, po kterém přestanou být DHT požadavky
         považovány za požadavky na bootstrap uzly. To se hodí zejm. pro omezení falešných detekcí v případech,
         kdy pcap soubor neobsahuje komunikaci od začátku. Nastavením na nulu se zobrazí pouze DHT uzly, jejichž
         adresa byla předtím překládána pomocí DNS.

-peers: vypíše seznam BitTorrent uzlů (peers) a případně DHT uzlů (nodes), ze kterých byly adresy peerů zjištěny,
        a také UDP trackerů, které peera ohlásily
        > pokud je použit přepínač -vp, vypíšou se všechny uzly, které se v komunikaci objevily, tj. i uzly, které
          byly vráceny v nějaké DHT odpovědi, ale nekomunikovalo se s nimi; jinak se vypíšou pouze uzly, od kterých
          přišla alespoň jedna část dat (piece)

-download: vypíše seznam stahovaných torrentů a uzly, které se na stahování podílely

-nodes: vypíše všechny zachycené DHT uzly a případně informace o peerech, které v nich byly nalezeny

---

Některé části kódu byly převzaty z https://github.com/elektito/bttools/ a upraveny.
Testovací data: https://drive.google.com/drive/folders/1xxJUkvanOKXdyviWrBy1otZR6tb5SWmU