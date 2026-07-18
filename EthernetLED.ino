/*MOSI D11 PB3
  MISO D12 PB4
  SCLK D13 PB5
  CS   D10 PB2 // Pin for W5500
  RST  RST 
  3V3  3V3 
  GND  GND 
  LED  D05 PD5
*/
// Arduino nano(ATMega328P),
// W5500
// srd-05vdc-sl-c

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <EEPROM.h>

//*****************************************************************************************************************************************************************************************
// START
//*****************************************************************************************************************************************************************************************
byte mac[6];
IPAddress ip;
IPAddress DNS(8, 8, 8, 8);
IPAddress Gateway(10, 0, 0, 1);
IPAddress subnet;
unsigned int localPort = 8888;  // local port to listen on

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];  //buffer to hold incoming packet

bool relayState = false;
bool isConfigured = false;

uint8_t readW5500PHY() {
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));  // SPI setup
  digitalWrite(10, LOW);                                            // CS to LOW to activate W5500 chip

  SPI.transfer(0x00);  // Address High
  SPI.transfer(0x2E);  // Address Low -- 0x002E = PHY Configuration Register
  SPI.transfer(0x00);  // Control byte, to read from address, not write

  uint8_t phy_status = SPI.transfer(0x00); // send empty byte so W5500 knows to
  digitalWrite(10, HIGH);  // CS to HIGH to deactivate W5500 chip
  SPI.endTransaction();
  return phy_status;
}

void setRelays(int state) {
  if (state) {
    relayState = true;
    digitalWrite(5, HIGH);
  } else {
    relayState = false;
    digitalWrite(5, LOW);
  }
}

void sendRelayStatus() {
  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());  // prepare to send package back to device that originally sent a package
  if (relayState) {  // this is message in packet,
    Udp.write("ON"); 
  } else {
    Udp.write("OFF");
  }
  Udp.endPacket(); // sends package back to python script, then script prints it with answer[Raw].load.decode()
  Serial.print(F("Relay status: ")); // print on serial
  if (relayState) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }
}

//*****************************************************************************************************************************************************************************************
// SETUP
//*****************************************************************************************************************************************************************************************
void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  pinMode(5, OUTPUT);
  Ethernet.init(10);     // use pin 10 for Ethernet CS
  // Check if device is configured
  if (EEPROM.read(14) == 0xAA) {
    isConfigured = true;
    for (int i = 0; i < 4; i++) ip[i] = EEPROM.read(i);
    for (int i = 0; i < 4; i++) subnet[i] = EEPROM.read(i + 4);
    for (int i = 0; i < 6; i++) mac[i] = EEPROM.read(i + 8);

    Ethernet.begin(mac, ip);  //start only if is configured
    Udp.begin(localPort);     // UDP waits to get a package

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.");
    } else if (Ethernet.hardwareStatus() == EthernetW5500) {
      Serial.println("W5500 Ethernet controller detected.");
      Serial.println("Press 'c' to enter menu.");
    }
  } else {  // not configured
    isConfigured = false;
    Serial.println("WARNING: The device isnt configured! W5500 will not start.");
    Serial.println("Press 'c' to enter menu.");
  }
}

//*****************************************************************************************************************************************************************************************
// LOOP
//*****************************************************************************************************************************************************************************************
void loop() {
  unsigned long timenow = millis();
  if (Serial.available() > 0) { // waiting if a button is pressed
    char Received_Char = Serial.read(); // read pressed button

    //*************************************************************************************************************************************************************************************
    // MENU
    //*************************************************************************************************************************************************************************************
    if (Received_Char == 'c') {
      delay(10);
      while (Serial.available() > 0) Serial.read(); //clear Serial.available
      Serial.println("1. Configure MAC adress");
      Serial.println("2. Configure IP address");
      Serial.println("3. Configure subnet");
      while (Serial.available() == 0) {} // wait until a button is pressed
      int number = Serial.parseInt(); // read a first number from serial
      delay(10);
      while (Serial.available() > 0) {
        Serial.read();
      }

      //***********************************************************************************************************************************************************************************
      // MAC ADDRESS CHANGE
      //***********************************************************************************************************************************************************************************
      if (number == 1) {
        int attempt = 0;
        bool success = false;
        while (attempt < 3 && !success) {
          Serial.print("\nEnter new MAC address (format: AA:BB:CC:DD:EE:FF): ");
          Serial.print(attempt + 1);
          Serial.println("/3]: ");
          while (Serial.available() == 0) {}  // wait until a button is pressed
          String inputMAC = Serial.readStringUntil('\n');
          inputMAC.trim();

          int m[6];
          if (sscanf(inputMAC.c_str(), "%x:%x:%x:%x:%x:%x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) { // turn string into array
            for (int i = 0; i < 6; i++) {
              mac[i] = (byte)m[i];
              EEPROM.update(8 + i, mac[i]);
            }
            EEPROM.update(14, 0xAA);
            Ethernet.begin(mac, ip);
            Udp.begin(localPort);
            isConfigured = true;
            Serial.println("MAC address successfully saved!");
            success = true;
          } else {
            attempt++;
            Serial.println("Invalid format.");
            if (attempt == 3) Serial.println("Too many incorrect entries!");
          }
        }

        //*********************************************************************************************************************************************************************************
        // IP ADDRESS CHANGE
        //*********************************************************************************************************************************************************************************
      } else if (number == 2) {
        int attempts = 0;
        bool success2 = false;

        while (attempts < 3 && !success2) {
          Serial.println("\nEnter new IP address (format: x.x.x.x): ");
          Serial.print(attempts + 1);
          Serial.println("/3]: ");
          while (Serial.available() == 0) {}
          String Input = Serial.readStringUntil('\n');
          Input.trim();
          Serial.print("Entered: ");
          Serial.println(Input);
          int b1, b2, b3, b4;
          if (sscanf(Input.c_str(), "%d.%d.%d.%d", &b1, &b2, &b3, &b4) == 4) {   // turn string into array
            if (b1 >= 0 && b1 <= 255 && b2 >= 0 && b2 <= 255 && b3 >= 0 && b3 <= 255 && b4 >= 0 && b4 <= 255) {
              IPAddress newIP(b1, b2, b3, b4);
              for (int i = 0; i < 4; i++) {
                EEPROM.update(i, newIP[i]);
              }
              ip = newIP;
              EEPROM.update(14, 0xAA);
              Ethernet.begin(mac, ip);
              Udp.begin(localPort);
              isConfigured = true;
              Serial.println("IP address successfully saved!");
              success2 = true;
            } else {
              attempts++;
              Serial.println("ERROR: Numbers must be between 0 and 255!");
              if (attempts == 3) Serial.println("Too many incorrect entries!");
            }
          } else {
            attempts++;
            Serial.println("ERROR: Invalid format. Use dots (e.g., 192.168.1.1).");
            if (attempts == 3) Serial.println("Too many incorrect entries!");
          }
        }

        //*********************************************************************************************************************************************************************************
        // SUBNET CHANGE
        //*********************************************************************************************************************************************************************************
      } else if (number == 3) {
        while (Serial.available()) { Serial.read(); }

        int attempt3 = 0;
        bool success3 = false;
        while (attempt3 < 3 && !success3) {
          Serial.println("\nEnter new subnet(format: x.x.x.x): ");
          Serial.print(attempt3 + 1);
          Serial.println("/3]: ");
          while (Serial.available() == 0) {}
          String newSubnet = Serial.readStringUntil('\n');
          newSubnet.trim();

          Serial.print("Entered: ");
          Serial.println(newSubnet);
          int s1, s2, s3, s4;
          if (sscanf(newSubnet.c_str(), "%d.%d.%d.%d", &s1, &s2, &s3, &s4) == 4) {

            if (s1 >= 0 && s1 <= 255 && s2 >= 0 && s2 <= 255 && s3 >= 0 && s3 <= 255 && s4 >= 0 && s4 <= 255) {
              IPAddress newSubnetObj(s1, s2, s3, s4);
              for (int i = 0; i < 4; i++) {
                EEPROM.update(4 + i, newSubnetObj[i]);
              }
              subnet = newSubnetObj;
              EEPROM.update(14, 0xAA);
              Ethernet.begin(mac, ip);
              Udp.begin(localPort);
              isConfigured = true;
              Serial.println("Subnet successfully saved!");
              success3 = true;
            } else {
              attempt3++;
              Serial.println("ERROR: Numbers must be between 0 and 255!");
              if (attempt3 == 3) Serial.println("Too many incorrect entries!");
            }
          } else {
            attempt3++;
            Serial.println("ERROR: Invalid format.");
            if (attempt3 == 3) Serial.println("Too many incorrect entries!");
          }
        }
      }
    }
  }
  // If the device isnt configured, exit loop
  if (!isConfigured) {
    static unsigned long timebefore = 0;
    const long interval = 5000;

    if (timenow - timebefore >= interval) { 
      Serial.println("Press 'c' to configure in Menu.");
      timebefore = timenow;
    }
    return;
  }

  //***************************************************************************************************************************************************************************************
  // PRINT ALL
  //***************************************************************************************************************************************************************************************

  static unsigned long timebefore = 0;
  const long interval = 5000;


  if (timenow - timebefore >= interval) {
    timebefore = timenow;
    if (Ethernet.linkStatus() == Unknown) {
      Serial.print("Link status unknown. Link status detection is only available with W5500. ");
    } else if (Ethernet.linkStatus() == LinkON) {
      Serial.print("Link settings: Up ");
      uint8_t phy_status = readW5500PHY();
      bool speed100 = (phy_status & (1 << 2));
      bool fullDuplex = (phy_status & (1 << 1));
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
    for (int i = 0; i < 6; i++) {
      if (mac[i] < 0x10) {
        Serial.print("0");
      }
      Serial.print(mac[i], HEX);
      if (i < 5) {
        Serial.print(":");
      }
    }
    Serial.print(", IP: ");
    Serial.print(ip);
    Serial.print("/");
    Serial.print(subnet);
    Serial.print(F(", Relays: "));
    if (relayState) {
      Serial.println("ON");
    } else {
      Serial.println("OFF");
    }
  }

  //***************************************************************************************************************************************************************************************
  // UDP
  //***************************************************************************************************************************************************************************************

  int packetSize = Udp.parsePacket();     // if there's data available, read a packet
  if (packetSize) {
    // Reading content into buffer
    int len = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE - 1);
    if (len > 0) {
      packetBuffer[len] = '\0';  // Converting byte array to proper text
    }
    // Converting to String and clearing invisible characters
    String Received_Data = String(packetBuffer);
    Received_Data.trim();
    // ID CHECK AND TURNING ON relay
    Received_Data.toUpperCase();
    if (Received_Data == "ON") {
      setRelays(1);
    } else if (Received_Data == "OFF") {
      setRelays(0);
    }
    sendRelayStatus();
  }
}