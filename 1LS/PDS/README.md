# Přenos dat, počítačové sítě a protokoly
### Ak. rok 2022/2023. [Karta předmětu](https://www.fit.vut.cz/study/course/259458/.cs).

## Projekt – Monitorování a detekce provozu BitTorrent
Hodnocení: 24 / 25

```
Dokumentace
- pěkná dokumentace, obsahuje vysvětlení, experimenty, odkazy 
- Popis BitTorrent - obsahuje popis architektury BT včetně popisu protokolů 
- podrobný popis experimentů s klientem BT 

Návrh detekce, implementace
- Detekce komunikace, výpis uzlů, vyhodnocení - není vhodné vypisovat hlášky typu "dejte si kávičku, za chvíli jsem hotov" 
- init: nedetekuje žádné bootstrap uzly v testovacím souboru init.pcapng 
- padá na testovacím souboru trackers.pcapng: python3 bt-monitor.py -pcap ../test/trackers.pcapng -init 
  Traceback (most recent call last): ... self.dns_possible_trackers[val] = name.decode() TypeError: unhashable type: 'list' 
```
