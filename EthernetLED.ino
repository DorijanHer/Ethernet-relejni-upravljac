#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <EEPROM.h>

/* ---------- Pinout ---------- */
#define W5500_CS_PIN 10
#define RELAY_PIN 5

/* ---------- EEPROM ---------- */
#define IP_LENGTH 4
#define SUBNET_LENGTH 4
#define MAC_LENGTH 6

#define EEPROM_IP_ADDR 0
#define EEPROM_SUBNET_ADDR 4
#define EEPROM_MAC_ADDR 8
#define EEPROM_CONFIG_FLAG_ADDR 14
#define CONFIGURED_FLAG_VALUE 0xAA

/* ---------- Serial ---------- */
#define SERIAL_BAUD_RATE 9600
#define INTERVAL_MS 5000

/* ---------- W5500 SPI frame ---------- */
#define W5500_SPI_SPEED 4000000
#define W5500_DUMMY_BYTE 0x00

#define W5500_BSB_SHIFT 3
#define W5500_RWB_READ 0x00
#define W5500_RWB_WRITE 0x04
#define W5500_OM_VDM 0x00
#define W5500_BSB_COMMON 0x00
#define W5500_BSB_SOCKET_REG(n) (((n) << 2) + 1)

#define W5500_PHYCFGR_ADDR_H 0x00
#define W5500_PHYCFGR_ADDR_L 0x2E
#define W5500_SN_MR_ADDR_H 0x00
#define W5500_SN_MR_ADDR_L 0x00

#define PHY_SPEED_100_MASK (1 << 1)
#define PHY_FULL_DUPLEX_MASK (1 << 2)

#define SN_MR_PROTOCOL_MASK 0x0F
#define SN_MR_PROTOCOL_UDP 0x02
#define SN_MR_UCASTB_MASK 0x10
#define SN_MR_BCASTB_MASK 0x40
#define SN_MR_MULTI_MASK 0x80

#define MAX_HEX 0x10
#define SERIAL_DELAY_MS 10
#define MAX_ATTEMPTS 3
#define MAX_NUM 255
#define MIN_NUM 0

/* GLOBALS */
byte mac[MAC_LENGTH];
IPAddress ip;
IPAddress subnet;
unsigned int localPort = 8888;

EthernetUDP Udp;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];
bool relayState = false;
bool isConfigured = false;

void setRelays(bool state) {
  if (state) {
    relayState = true;
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    relayState = false;
    digitalWrite(RELAY_PIN, LOW);
  }
}

void sendRelayStatus() {
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  if (relayState) {
    Udp.write("ON");
  } else {
    Udp.write("OFF");
  }
  Udp.endPacket();
  Serial.print("Relay status: ");
  if (relayState) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }
}

/**
 * @brief disabling receiving multicast and broadcast
 **/
static uint8_t w5500Read(uint8_t bsb, uint8_t addrHigh, uint8_t addrLow) {
  uint8_t control = (bsb << W5500_BSB_SHIFT) | W5500_RWB_READ | W5500_OM_VDM;

  SPI.beginTransaction(SPISettings(W5500_SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(W5500_CS_PIN, LOW);
  SPI.transfer(addrHigh);
  SPI.transfer(addrLow);
  SPI.transfer(control);
  /* Dummy byte only clocks SCLK so the W5500 can shift its answer out. */
  uint8_t value = SPI.transfer(W5500_DUMMY_BYTE);
  digitalWrite(W5500_CS_PIN, HIGH);
  SPI.endTransaction();

  return value;
}

static void w5500Write(uint8_t bsb, uint8_t addrHigh, uint8_t addrLow, uint8_t value) {
  uint8_t control = (bsb << W5500_BSB_SHIFT) | W5500_RWB_WRITE | W5500_OM_VDM;

  SPI.beginTransaction(SPISettings(W5500_SPI_SPEED, MSBFIRST, SPI_MODE0));
  digitalWrite(W5500_CS_PIN, LOW);
  SPI.transfer(addrHigh);
  SPI.transfer(addrLow);
  SPI.transfer(control);
  SPI.transfer(value);
  digitalWrite(W5500_CS_PIN, HIGH);
  SPI.endTransaction();
}
static uint8_t readW5500PHY(void) {
  return w5500Read(W5500_BSB_COMMON, W5500_PHYCFGR_ADDR_H, W5500_PHYCFGR_ADDR_L);
}

static uint8_t readSnMR(uint8_t socket) {
  return w5500Read(W5500_BSB_SOCKET_REG(socket), W5500_SN_MR_ADDR_H, W5500_SN_MR_ADDR_L);
}

static void writeSnMR(uint8_t socket, uint8_t value) {
  w5500Write(W5500_BSB_SOCKET_REG(socket), W5500_SN_MR_ADDR_H, W5500_SN_MR_ADDR_L, value);
}
void blockUDPBroadcastAndMulticast(void) {
  for (uint8_t n = 0; n < MAX_SOCK_NUM; n++) {
    uint8_t mr = readSnMR(n);

    /* block broadcast   */
    mr |= SN_MR_BCASTB_MASK;
    /* disable multicast */
    mr &= (uint8_t)~SN_MR_MULTI_MASK;
    /* make sure unicast stays allowed */
    mr &= (uint8_t)~SN_MR_UCASTB_MASK;
    writeSnMR(n, mr);
  }
}

static void startNetwork() {
  Ethernet.begin(mac, ip);
  Ethernet.setSubnetMask(subnet);
  Udp.begin(localPort);
  blockUDPBroadcastAndMulticast();
}

void setup() {
  Serial.begin(SERIAL_BAUD_RATE);
  pinMode(RELAY_PIN, OUTPUT);
  Ethernet.init(W5500_CS_PIN);
  /* Check if device is configured */
  if (EEPROM.read(EEPROM_CONFIG_FLAG_ADDR) == CONFIGURED_FLAG_VALUE) {
    isConfigured = true;
    for (int i = 0; i < IP_LENGTH; i++) {
      ip[i] = EEPROM.read(i + EEPROM_IP_ADDR);
    }
    for (int i = 0; i < SUBNET_LENGTH; i++) {
      subnet[i] = EEPROM.read(i + EEPROM_SUBNET_ADDR);
    }
    for (int i = 0; i < MAC_LENGTH; i++) {
      mac[i] = EEPROM.read(i + EEPROM_MAC_ADDR);
    }
    startNetwork();
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.");
    } else if (Ethernet.hardwareStatus() == EthernetW5500) {
      Serial.println("W5500 Ethernet controller detected.");
      Serial.println("Press 'c' to enter menu.");
    }
    /* not configured*/
  } else {
    isConfigured = false;
    Serial.println("WARNING: The device isn't configured! W5500 will not start.");
    Serial.println("Press 'c' to enter menu.");
  }
}

void loop() {
  unsigned long timenow = millis();
  if (Serial.available() > 0) {
    char Received_Char = Serial.read();

    /* MENU */
    if (Received_Char == 'c') {
      delay(SERIAL_DELAY_MS);
      while (Serial.available() > 0) {
        Serial.read();
      }
      Serial.println("1. Configure MAC address");
      Serial.println("2. Configure IP address");
      Serial.println("3. Configure subnet");
      while (Serial.available() == 0) {}
      int number = Serial.parseInt();
      delay(SERIAL_DELAY_MS);
      while (Serial.available() > 0) {
        Serial.read();
      }

      /* MAC ADDRESS CHANGE */
      if (number == 1) {
        int attempt = 0;
        bool success = false;
        while (attempt < MAX_ATTEMPTS && !success) {
          Serial.print("\nEnter new MAC address (format: AA:BB:CC:DD:EE:FF): ");
          Serial.print(attempt + 1);
          Serial.println("/3]: ");
          while (Serial.available() == 0) {}
          String inputMAC = Serial.readStringUntil('\n');
          inputMAC.trim();
          int m[MAC_LENGTH];
          /* turn string into array */
          if (sscanf(inputMAC.c_str(), "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == MAC_LENGTH) {
            for (int i = 0; i < MAC_LENGTH; i++) {
              mac[i] = (byte)m[i];
              EEPROM.update(EEPROM_MAC_ADDR + i, mac[i]);
            }
            EEPROM.update(EEPROM_CONFIG_FLAG_ADDR, CONFIGURED_FLAG_VALUE);
            startNetwork();
            isConfigured = true;
            Serial.println("MAC address successfully saved!");
            success = true;
          } else {
            attempt++;
            Serial.println("Invalid format.");
            if (attempt == MAX_ATTEMPTS) {
              Serial.println("Too many incorrect entries!");
            }
          }
        }

        /* IP ADDRESS CHANGE */
      } else if (number == 2) {
        int attempts = 0;
        bool success2 = false;
        while (attempts < MAX_ATTEMPTS && !success2) {
          Serial.println("\nEnter new IP address (format: x.x.x.x): ");
          Serial.print(attempts + 1);
          Serial.println("/3]: ");
          while (Serial.available() == 0) {}
          String Input = Serial.readStringUntil('\n');
          Input.trim();
          Serial.print("Entered: ");
          Serial.println(Input);
          int b1, b2, b3, b4;
          if (sscanf(Input.c_str(), "%d.%d.%d.%d", &b1, &b2, &b3, &b4) == IP_LENGTH) {
            if (b1 >= MIN_NUM && b1 <= MAX_NUM && b2 >= MIN_NUM && b2 <= MAX_NUM && b3 >= MIN_NUM && b3 <= MAX_NUM && b4 >= MIN_NUM && b4 <= MAX_NUM) {
              IPAddress newIP(b1, b2, b3, b4);
              for (int i = 0; i < IP_LENGTH; i++) {
                EEPROM.update(EEPROM_IP_ADDR + i, newIP[i]);
              }
              ip = newIP;
              EEPROM.update(EEPROM_CONFIG_FLAG_ADDR, CONFIGURED_FLAG_VALUE);
              startNetwork();
              isConfigured = true;
              Serial.println("IP address successfully saved!");
              success2 = true;
            } else {
              attempts++;
              Serial.println("ERROR: Numbers must be between 0 and 255!");
              if (attempts == MAX_ATTEMPTS) {
                Serial.println("Too many incorrect entries!");
              }
            }
          } else {
            attempts++;
            Serial.println("ERROR: Invalid format. Use dots (e.g., 192.168.1.1).");
            if (attempts == MAX_ATTEMPTS) Serial.println("Too many incorrect entries!");
          }
        }

        /* SUBNET CHANGE */
      } else if (number == 3) {
        while (Serial.available()) {
          Serial.read();
        }
        int attempt3 = 0;
        bool success3 = false;
        while (attempt3 < MAX_ATTEMPTS && !success3) {
          Serial.println("\nEnter new subnet(format: x.x.x.x): ");
          Serial.print(attempt3 + 1);
          Serial.println("/3]: ");
          while (Serial.available() == 0) {}
          String newSubnet = Serial.readStringUntil('\n');
          newSubnet.trim();
          Serial.print("Entered: ");
          Serial.println(newSubnet);
          int s1, s2, s3, s4;
          if (sscanf(newSubnet.c_str(), "%d.%d.%d.%d", &s1, &s2, &s3, &s4) == SUBNET_LENGTH) {
            if (s1 >= MIN_NUM && s1 <= MAX_NUM && s2 >= MIN_NUM && s2 <= MAX_NUM && s3 >= MIN_NUM && s3 <= MAX_NUM && s4 >= MIN_NUM && s4 <= MAX_NUM) {
              IPAddress newSubnetObj(s1, s2, s3, s4);
              for (int i = 0; i < SUBNET_LENGTH; i++) {
                EEPROM.update(EEPROM_SUBNET_ADDR + i, newSubnetObj[i]);
              }
              subnet = newSubnetObj;
              EEPROM.update(EEPROM_CONFIG_FLAG_ADDR, CONFIGURED_FLAG_VALUE);
              startNetwork();
              isConfigured = true;
              Serial.println("Subnet successfully saved!");
              success3 = true;
            } else {
              attempt3++;
              Serial.println("ERROR: Numbers must be between 0 and 255!");
              if (attempt3 == MAX_ATTEMPTS) {
                Serial.println("Too many incorrect entries!");
              }
            }
          } else {
            attempt3++;
            Serial.println("ERROR: Invalid format.");
            if (attempt3 == MAX_ATTEMPTS) Serial.println("Too many incorrect entries!");
          }
        }
      }
    }
  }

  /* If the device isnt configured, exit loop */
  if (!isConfigured) {
    static unsigned long timebefore = 0;
    if (timenow - timebefore >= INTERVAL_MS) {
      Serial.println("Press 'c' to configure in Menu.");
      timebefore = timenow;
    }
    return;
  }

  /* PRINT STATUS */
  static unsigned long timebefore = 0;
  if (timenow - timebefore >= INTERVAL_MS) {
    timebefore = timenow;
    if (Ethernet.linkStatus() == Unknown) {
      Serial.print("Link status unknown. Link status detection is only available with W5500. ");
    } else if (Ethernet.linkStatus() == LinkON) {
      Serial.print("Link settings: Up ");
      uint8_t phy_status = readW5500PHY();
      bool speed100 = (phy_status & PHY_SPEED_100_MASK);
      bool fullDuplex = (phy_status & PHY_FULL_DUPLEX_MASK);
      if (speed100) {
        Serial.print("100/");
      } else {
        Serial.print("10/");
      }
      if (fullDuplex) {
        Serial.print("Full, ");
      } else {
        Serial.print("Half, ");
      }
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.print("Link settings: Down ");
    }
    Serial.print("MAC: ");
    for (int i = 0; i < MAC_LENGTH; i++) {
      if (mac[i] < MAX_HEX) {
        Serial.print("0");
      }
      Serial.print(mac[i], HEX);
      if (i < MAC_LENGTH - 1) {
        Serial.print(":");
      }
    }
    Serial.print(", IP: ");
    Serial.print(ip);
    Serial.print("/");
    Serial.print(subnet);
    Serial.print(", Relays: ");
    if (relayState) {
      Serial.println("ON");
    } else {
      Serial.println("OFF");
    }
  }

  /* UDP */
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    int len = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE - 1);
    if (len > 0) {
      packetBuffer[len] = '\0';
    }
    String Received_Data = String(packetBuffer);
    Received_Data.trim();
    /* ID CHECK AND TURNING ON relay */
    Received_Data.toUpperCase();
    if (Received_Data == "ON") {
      setRelays(true);
      sendRelayStatus();
    } else if (Received_Data == "OFF") {
      setRelays(false);
      sendRelayStatus();
    } else if (Received_Data == "STATUS") {
      sendRelayStatus();
    }
  }
}
