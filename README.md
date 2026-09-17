# Ethernet relejni upravljač

Uređaj za daljinsko uključivanje i isključivanje trošila putem lokalne Ethernet mreže. Izgrađen je oko razvojne pločice Arduino Nano V3 (ATmega328P) i Ethernet modula WIZnet W5500. Dvama relejnim modulima upravlja se UDP naredbama na priključku 8888, a mrežne postavke (MAC, IP, maska podmreže) unose se serijskim terminalom i trajno pohranjuju u EEPROM.

Projekt je izrađen kao završni rad na Tehničkom veleučilištu u Zagrebu.
## Značajke

- Upravljanje dvama relejnim modulima UDP naredbama `ON`, `OFF` i `STATUS`
- Odgovor s trenutačnim stanjem releja na svaku prepoznatu naredbu
- Konfiguracija MAC adrese, IP adrese i maske podmreže preko serijskog izbornika (9600 bauda)
- Postavke se čuvaju u EEPROM-u i ostaju sačuvane nakon prekida napajanja
- Filtriranje broadcast i multicast prometa na razini Ethernet sklopa (registar `Sn_MR`)
- Periodički ispis stanja: veza, brzina i dupleks (iz registra `PHYCFGR`), MAC, IP i stanje releja
- Neblokirajuća glavna petlja — uređaj je uvijek dostupan za dolazne naredbe
- Napajanje neovisno o računalu, iz USB-C punjača
- Kućište izrađeno trodimenzionalnim ispisom, STL datoteke u repozitoriju
- Fiksni UDP priključak: 8888


## Arhitektura sustava


Računalo u lokalnoj mreži šalje kratke UDP poruke na priključak 8888. W5500 prima pakete i predaje ih mikroupravljaču preko SPI sučelja. Mikroupravljač tumači naredbu, postavlja izlaz D5 kojim su pobuđena oba relejna modula te pošiljatelju vraća poruku s trenutačnim stanjem releja.

Napajanje je odvojeno od USB priključka računala: modul HUSB238 uspostavlja sabirnicu od 5 V, a linearni regulator LM1084 iz nje izvodi 3,3 V za W5500.


## Sklopovlje

### Popis komponenata

| Komponenta 			                     |        Količina 	    |
| --------------------------------------  | ---------------------  |
| Arduino Nano V3 (ATmega328P)            |           1            |
| WIZnet W5500 Ethernet modul             |           1            |
| Relejni modul COM-RM01 (SRD-05VDC-SL-C) |           2            |
| HUSB238 USB-C PD ploča                  |           1            |
| Regulator LM1084IT-3.3                  |           1            |
| Elektrolitski kondenzator 47 µF / 16 V  |           4            |

### Arhitektura napajanja
Izvod 3V3 na pločici Arduino Nano napaja se iz internog regulatora i može isporučiti samo 25–30 mA, dok W5500 tipično troši do 130 mA. Zbog toga se W5500 ne napaja s pločice, nego je uveden zaseban izvor HUSB238 (USB-C PD, 5 V / 1 A). On napaja 5V sabirnicu na koju su spojeni Arduino Nano, oba relejna modula i ulaz regulatora LM1084, čiji izlaz 3V3 služi isključivo za W5500.

Uz regulator su ugrađena četiri elektrolitska kondenzatora 47 µF / 16 V: dva paralelno na ulazu (5 V – GND) i dva paralelno na izlazu (3V3 – GND); izlazni kondenzator obvezan je za stabilnost regulatora. Sve komponente dijele zajedničku masu.

| Komponenta       | Napon | Struja     | Max. struja |
| ---------------  | ----- | ---------- |  ---------  |
| W5500            | 3V3   | 130 mA     | 180 mA      |
| Arduino Nano     | 5 V   | 30 mA      | 50 mA       |
| 2× relejni modul | 5 V   | 0 mA       | 150 mA      |
| LM1084           | ----- | 5 mA       | 10 mA       |
| Ukupno           | ----- | 165 mA     | 390 mA      |

### Povezivanje

| W5500 | Arduino Nano |
| ------| ------------ |
| MOSI  | D11 (PB3)    |
| MISO  | D12 (PB4)    |
| SCLK  | D13 (PB5)    |
| SCS   | D10 (PB2)    |
| RST   | RST          |

Oba relejna modula spojena su na pin D5 (PD5), zajedničku masu i 5V sabirnicu. Shema spajanja nalazi se u tehničkoj dokumentaciji.

### Kućište

Kućište je modelirano u programu Fusion 360 i izrađeno trodimenzionalnim ispisom iz PLA materijala. Vanjske dimenzije iznose 133 × 113 mm uz visinu od 38,5 mm. Poklopac se pričvršćuje vijcima M3 u navojne umetke utisnute lemilicom. Na stijenkama su izrezi za RJ45, USB-C, mini-USB i vijčane stezaljke releja.

Modeli: `Kuciste5.stl` i `Poklopac5.stl`.

## Firmware

`EthernetLED.ino` koristi standardne Arduino biblioteke `SPI`, `Ethernet`, `EthernetUdp` i `EEPROM`.

Biblioteka Ethernet ne otkriva sve registre modula, pa su napisane pomoćne funkcije koje im pristupaju izravnom SPI komunikacijom:

- **`PHYCFGR` (0x002E)** — očitanje stanja fizičke veze: je li veza uspostavljena, brzina (10/100 Mbit/s) i dupleks
- **`Sn_MR`** — filtriranje prometa: postavlja se bit `BCASTB` (0x40) i briše bit `MULTI` (0x80), čime se broadcast i multicast paketi odbacuju u samom Ethernet sklopu, prije nego dođu do programa

Glavna petlja izvodi se neblokirajuće, funkcijom `millis()` umjesto `delay()`, pa uređaj između dva periodička ispisa nije nedostupan za dolazne naredbe.

### EEPROM mapa
| Adresa | Sadržaj                        |
| ------ | --------------------------     |
| 0–3    | IP adresa                      |
| 4–7    | Subnet                         |
| 8–13   | MAC adresa                     |
| 14     | Zastavica konfiguracije (0xAA) |

### Prvo pokretanje i konfiguracija

Potrebna dodatna oprema: USB-C punjač, Ethernet kabel i mini-USB kabel za konfiguraciju.

1. Priključite Ethernet kabel, USB-C napajanje te mini-USB kabelom povežite Arduino i računalo.
2. Otvorite serijski terminal na pripadajućem COM portu, brzina 9600 baud.
3. Nekonfiguriran uređaj javlja "WARNING: The device isnt configured! W5500 will not start" i svakih 5 sekundi podsjeća porukom "Press 'c' to enter menu."
4. Pritiskom na "c" otvara se izbornik:
   - 1. Configure MAC adress — unos u obliku AA:BB:CC:DD:EE:FF
   - 2. Configure IP address — unos u obliku x.x.x.x
   - 3. Configure subnet     — unos u obliku x.x.x.x
5. Svaki uspješan unos potvrđuje se porukom "... successfully saved!". Za svaki unos dopuštena su 3 pokušaja, a neispravna sintaksa javlja grešku.

Konfiguriran uređaj svakih 5 sekundi ispisuje redak sa stanjem postavki:


Link settings: Up 100/Full, MAC: AA:BB:CC:DD:EE:FF, IP: 10.0.0.50/255.255.255.0, Relays: OFF


## Upravljanje relejima

### UDP naredbe

| Naredba | Učinak                | Odgovor  |
| ------  | --------------------- | -------  |
| ON      | Uključuje oba releja  | ON       |
| OFF     | Isključuje oba releja | OFF      |
| STATUS  | Ne mijenja stanje     | ON / OFF |

### Upravljanje Python skriptom


1. Instalirajte biblioteku Scapy: `pip install scapy`
2. Na sustavu Windows instalirajte i upravljački program [Npcap](https://npcap.com/) — Scapy šalje pakete putem sirovih priključnica
3. U skripti `ScriptRelay.py` podesite varijable `ip` i `port`
4. Otvorite naredbeni redak **s administratorskim ovlastima** i pozicionirajte se u mapu sa skriptom
5. Pokrenite skriptu s naredbom kao argumentom:

python ScriptRelay.py on

Ovisno o naredbi, releji će se upaliti ili ugasiti, a skripta ispisuje trenutačni status ("Relay status: ...").

## Sigurnosne napomene

> ⚠️ Kontakti releja smiju se opteretiti s najviše 10 A / 250 V AC, odnosno 10 A / 30 V DC. Ako se releji spajaju na visoki napon (230 V), potreban je velik oprez zbog opasnosti od strujnog udara.

## Sadržaj repozitorija

| Datoteka                       | Opis                                 |
| ---------------------------    | ------------------------------       |
| `EthernetLED.ino`              | Firmware za Arduino Nano             |
| `ScriptRelay.py`               | Python skripta za slanje UDP naredbi |
| `Kuciste5.stl`                 | 3D model kućišta                     |
| `Poklopac5.stl`                | 3D model poklopca                    |
| `Zavrsni_Rad.docx`             | Završni rad                          |
