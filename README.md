# Ethernet relejni upravljač

# 

Uređaj za uključivanje i isključivanje trošila preko lokalne mreže. Sastoji se od mikroupravljača Arduino Nano V3 (ATmega328P) i Ethernet modula WIZnet W5500, s dva relejna modula upravlja UDP porukama na portu 8888. Mrežne postavke (MAC, IP, subnet) konfiguriraju se serijskim terminalom i trajno spremaju u EEPROM.

# 

## Značajke

# 

\- Upravljanje s dva relejna modula UDP naredbama ON, OFF i STATUS

\- Konfiguracija MAC adrese, IP adrese i subneta preko serijskog izbornika (9600 baud)

\- Postavke se čuvaju u EEPROM-u i ostaju sačuvane nakon nestanka struje

\- Periodični ispis stanja: link, brzina/duplex (čitano iz PHY registra W5500), MAC, IP i stanje releja

\- Fiksni UDP port: 8888





## Arhitektura sustava

# 

Računalo u lokalnoj mreži šalje kratke UDP poruke na port 8888. W5500 prima pakete i predaje ih mikroupravljaču SPI protokolom. Mikroupravljač tumači naredbe, postavlja izlaz D5 kojim su upravljana oba relejna modula te pošiljatelju vraća poruku s trenutačnim stanjem releja.



Napajanje je odvojeno od USB priključka računala: HUSB238 izvor daje 5 V sabirnici, a regulator LM1084 izvodi 3V3 za W5500.

# 

## Sklopovlje

# 

### Popis komponenata

# 

| Komponenta 			          |        Količina 	   |


| Arduino Nano V3 (ATmega328P)            |           1            |

| WIZnet W5500 Ethernet modul             |           1            |

| Relejni modul COM-RM01 (SRD-05VDC-SL-C) |           2            |

| HUSB238 USB-C PD ploča                  |           1            |

| Regulator LM1084IT-3.3                  |           1            |

| Elektrolitski kondenzator 47 µF / 16 V  |           4            |

# 

### Arhitektura napajanja

# 

Izvod 3V3 na pločici Arduino Nano napaja se iz internog regulatora i može isporučiti samo 25–30 mA, dok W5500 tipično troši do 130 mA. Zbog toga se W5500 ne napaja s pločice, nego je uveden zaseban izvor HUSB238 (USB-C PD, 5 V / 1 A). On napaja 5V sabirnicu na koju su spojeni Arduino Nano, oba relejna modula i ulaz regulatora LM1084, čiji izlaz 3V3 služi isključivo za W5500.



Uz regulator su ugrađena četiri elektrolitska kondenzatora 47 µF / 16 V: dva paralelno na ulazu (5 V – GND) i dva paralelno na izlazu (3V3 – GND); izlazni kondenzator obvezan je za stabilnost regulatora. Sve komponente dijele zajedničku masu.

# 

| Komponenta       | Napon | Struja     | Max. struja |


| W5500            | 3V3   | 130 mA     | 180 mA      |

| Arduino Nano     | 5 V   | 30 mA      | 50 mA       |

| 2× relejni modul | 5 V   | 0 mA       | 150 mA      |

| LM1084           | ----- | 5 mA       | 10 mA       |

| Ukupno           | ----- | 165 mA     | 390 mA      |

# 

### Povezivanje

# 

| W5500 | Arduino Nano |

| MOSI  | D11 (PB3)    |

| MISO  | D12 (PB4)    |

| SCLK  | D13 (PB5)    |

| SCS   | D10 (PB2)    |

| RST   | RST          |

# 

Oba relejna modula spojena su na pin D5 (PD5), zajedničku masu i 5V sabirnicu. Shema spajanja nalazi se u tehničkoj dokumentaciji.

# 

## Firmware

# 

"EthernetLED.ino" koristi standardne Arduino biblioteke: "SPI", "Ethernet", "EthernetUdp" i "EEPROM". Brzina/duplex linka očitavaju se izravno iz PHY konfiguracijskog registra W5500 (adresa 0x002E) preko SPI-ja.

# 

### EEPROM mapa

# 

| Adresa | Sadržaj                        |

| 0–3    | IP adresa                      |

| 4–7    | Subnet                         |

| 8–13   | MAC adresa                     |

| 14     | Zastavica konfiguracije (0xAA) |

# 

### Prvo pokretanje i konfiguracija

# 

Potrebna dodatna oprema: USB-C punjač, Ethernet kabel i mini-USB kabel za konfiguraciju.



1\. Priključite Ethernet kabel, USB-C napajanje te mini-USB kabelom povežite Arduino i računalo.

2\. Otvorite serijski terminal na pripadajućem COM portu, brzina 9600 baud.

3\. Nekonfiguriran uređaj javlja "WARNING: The device isnt configured! W5500 will not start" i svakih 5 sekundi podsjeća porukom "Press 'c' to enter menu."

4\. Pritiskom na "c" otvara se izbornik:

&#x20;  - 1. Configure MAC adress — unos u obliku AA:BB:CC:DD:EE:FF

&#x20;  - 2. Configure IP address — unos u obliku x.x.x.x

&#x20;  - 3. Configure subnet     — unos u obliku x.x.x.x

5\. Svaki uspješan unos potvrđuje se porukom "... successfully saved!". Za svaki unos dopuštena su 3 pokušaja, a neispravna sintaksa javlja grešku.



Konfiguriran uređaj svakih 5 sekundi ispisuje redak sa stanjem postavki:





Link settings: Up 100/Full, MAC: AA:BB:CC:DD:EE:FF, IP: 10.0.0.50/255.255.255.0, Relays: OFF



# 

## Upravljanje relejima

# 

### UDP naredbe

# 

| Naredba | Učinak                | Odgovor  |


| ON      | Uključuje oba releja  | ON       |

| OFF     | Isključuje oba releja | OFF      |

| STATUS  | Ne mijenja stanje     | ON / OFF |

# 

### Upravljanje Python skriptom



1\. Instalirajte biblioteku scapy: "pip install scapy"

2\. U skripti podesite varijable "ip" i "port"

3\. U terminalu navigirajte do mape sa skriptom

4\. Pokrenite skriptu s naredbom kao argumentom:



python ScriptRelay.py on



Ovisno o naredbi, releji će se upaliti ili ugasiti, a skripta ispisuje trenutačni status ("Relay status: ...").

# 

## Sigurnosne napomene

# 

> ⚠️ Kontakti releja smiju se opteretiti s najviše 10 A / 250 V AC, odnosno 10 A / 30 V DC. Ako se releji spajaju na visoki napon (230 V), potreban je velik oprez zbog opasnosti od strujnog udara.

# 

## Sadržaj repozitorija

# 

| Datoteka                       | Opis                                                            |


| EthernetLED.ino                | Firmware za Arduino Nano                                        |

| Tehnicka\_dokumentacija.docx   | Tehnička dokumentacija — arhitektura sustava, sklopovlje, shema |

| korisnicka\_dokumentacija.docx | Korisnička dokumentacija — konfiguracija i upravljanje          |

