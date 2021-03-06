/*******************************************************************************
 * Copyright (c) 2015 Thomas Telkamp and Matthijs Kooijman
 *
 * Permission is hereby granted, free of charge, to anyone
 * obtaining a copy of this document and accompanying files,
 * to do whatever they want with them without any restriction,
 * including, but not limited to, copying, modification and redistribution.
 * NO WARRANTY OF ANY KIND IS PROVIDED.
 * 
 * This uses ABP (Activation-by-personalisation), where a DevAddr and
 * Session keys are preconfigured (unlike OTAA, where a DevEUI and
 * application key is configured, while the DevAddr and session keys are
 * assigned/generated in the over-the-air-activation procedure).
 *
 * Note: LoRaWAN per sub-band duty-cycle limitation is enforced (1% in
 * g1, 0.1% in g2), but not the TTN fair usage policy (which is probably
 * violated by this sketch when left running for longer)!
 *
 * To use this sketch, first register your application and device with
 * the things network, to set or generate a DevAddr, NwkSKey and
 * AppSKey. Each device should have their own unique values for these
 * fields.
 *
 * Do not forget to define the radio type correctly in config.h.
 *
 *******************************************************************************
 *
 * TTN Nijmegen localisation workshop with differen technologies
 * by Roger Meijs and Rob Jansen with special thanks to former workshops made by Frank Beks
 * 
 * This sketch will check it's location by listening to surrounding WiFi networks.
 * It will run on Wemos ESP8266 / RFM95 Ch2i board
 * 
 * Therefore use this LMIC library: https://github.com/ch2i/arduino-lmic 
 * this because of the special DIO settings (all pins tied together with diodes)
 * 
 * Payload function on TTN:
 * 
 * 
var hexChar = ["0", "1", "2", "3", "4", "5", "6", "7","8", "9", "A", "B", "C", "D", "E", "F"];
 
function byteToHex(b) {
  return hexChar[(b >> 4) & 0x0f] + hexChar[b & 0x0f];
}
function hexToInt(hex) {
  var num=hex;
  if (num>0x7F) {
    num=num-0x100;
  }
  return num;
}
function Decoder(bytes) {
  var mac1="";
  for (i = 0; i < 6; i++) { 
    mac1 += byteToHex(bytes[i]);
    if (i<5) { mac1+=':';}
}
var rssi1=hexToInt(bytes[6]);
  var mac2="";
  for (i = 0; i < 6; i++) { 
    mac2 += byteToHex(bytes[i+7]);
    if (i<5) { mac2+=':';}
}
var rssi2=hexToInt(bytes[13]);
 return {
    macaddress: {
      mac_1: mac1,
      rssi_1:rssi1,
      mac_2: mac2,
      rssi_2:rssi2,
    },
   };
}

 *********************************************************************************/

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <ESP8266WiFi.h>

/* Serial Baud Rate */
#define SERIAL_BAUD       115200
/* Delay paramter for connection. */
#define WIFI_DELAY        500
/* Max SSID octets. */
#define MAX_SSID_LEN      32
/* Wait this much until device gets IP. */
#define MAX_CONNECT_TIME  30000

/* SSID that to be stored to connect. */
char ssid[MAX_SSID_LEN] = "";

// LoRaWAN NwkSKey, network session key
// your TTN key in msb format
static const PROGMEM u1_t NWKSKEY[16] = {  };

// LoRaWAN AppSKey, application session key
// your ttn application session key in msb format
static const u1_t PROGMEM APPSKEY[16] = {  };

// LoRaWAN end-device address (DevAddr)
// copy your device address and put 0x before because its hex
static const u4_t DEVADDR = 0xYOURDEVICEADRESS ; // <-- Change this address for every node!

// These callbacks are only used in over-the-air activation, so they are
// left empty here (we cannot leave them out completely unless
// DISABLE_JOIN is set in config.h, otherwise the linker will complain).
void os_getArtEui (u1_t* buf) { }
void os_getDevEui (u1_t* buf) { }
void os_getDevKey (u1_t* buf) { }


#define MAX_MAC 2 // number of MAC addresses to sent
static uint8_t mydata[MAX_MAC*7] ;
static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

const lmic_pinmap lmic_pins = {
    .nss = 16,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = LMIC_UNUSED_PIN,
    .dio = {15, LMIC_UNUSED_PIN, LMIC_UNUSED_PIN},
};

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        case EV_SCAN_FOUND:
            Serial.println(F("EV_SCAN_FOUND"));
            break;
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            break;
         default:
            Serial.print(F("Unknown event: "));
            Serial.println(ev);
            break;
    }
}
void do_send(osjob_t* j){
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        // Prepare upstream data transmission at the next possible time.
          scanAndSort();
        LMIC_setTxData2(1, mydata, MAX_MAC*7, 0);  //Sent packet to TTN
        Serial.println(F("Packet queued"));
    }
    // Next TX is scheduled after TX_COMPLETE event.
}


/* Scan available networks and sort them in order to their signal strength. */
void scanAndSort() {
  memset(ssid, 0, MAX_SSID_LEN);
  uint8_t * macptr;
  int n = WiFi.scanNetworks();
  Serial.println("Scan done, sort starting!");
  if (n == 0) {
    Serial.println("No networks found!");
  } else {
    Serial.print(n);
    Serial.println(" networks found.");
    int indices[n];
    for (int i = 0; i < n; i++) {
      indices[i] = i;
    }
    for (int i = 0; i < n; i++) {
      for (int j = i + 1; j < n; j++) {
        if (WiFi.RSSI(indices[j]) > WiFi.RSSI(indices[i])) {
          std::swap(indices[i], indices[j]);
        }
      }
    }

    for (int i = 0; i < n; ++i) {
      Serial.print(WiFi.SSID(indices[i]));
      Serial.print(" ");
      Serial.print(WiFi.RSSI(indices[i]));
      Serial.print(" ");
      Serial.print(WiFi.BSSIDstr(indices[i]));
      Serial.print(" ");
      Serial.print(WiFi.encryptionType(indices[i]));
      Serial.println();
      }
      Serial.println("Best BSSID: ");
      for(int i=0;i<MAX_MAC;i++)
      {
      macptr=WiFi.BSSID(indices[i]);
      for(int j=0;j<6;j++) {
        Serial.printf("%02x",*macptr);
        if (j<5)
        {
          Serial.print(':');
        }
        mydata[j+7*i]=*macptr++;
    }
    Serial.print(" RSSI: ");
    mydata[6+7*i]=WiFi.RSSI(indices[i]);
    Serial.print(WiFi.RSSI(indices[i]));
    Serial.println();
  }
  }
}

void setup() {
  int k;
  Serial.begin(SERIAL_BAUD);
  Serial.println("Started....");
    Serial.println(F("Starting WiFi Localisation - TTN"));
  Serial.println( "Compiled: " __DATE__ ", " __TIME__);
  Serial.println(F("LMIC settings:"));
#ifdef DISABLE_PING
  Serial.println(F("- ping disabled"));
#else
  Serial.println(F("- ping enabled"));
#endif
#ifdef DISABLE_BEACONS
  Serial.println(F("- beacon disabled"));
#else
  Serial.println(F("- beacon enabled"));
#endif
#ifdef DISABLE_JOIN
  Serial.println(F("- join disabled"));
#else
  Serial.println(F("- join enabled"));
#endif
#ifdef LMIC_FAILURE_TO
  Serial.println(F("- LMIC_FAILURE TO enabled"));
#else
  Serial.println(F("- LMIC_FAILURE_TO disabled"));
#endif
#ifdef LMIC_PRINTF_TO
  Serial.println(F("- LMIC_PRINTF TO enabled"));
#else
  Serial.println(F("- LMIC_PRINTF_TO disabled"));
#endif
#ifdef LMIC_SPI_FREQ
  Serial.print(F("- LMIC_SPI_FREQ set to "));
  Serial.println(LMIC_SPI_FREQ);
#else
  Serial.println(F("- ****************************************************"));
  Serial.println(F("- ****************************************************"));
  Serial.println(F("- *** No LMIC_SPI_FREQ set, probably wrong LMIC!!! ***"));
  Serial.println(F("- ****************************************************"));
  Serial.println(F("- ****************************************************"));
#endif
 

        // LMIC init
        os_init();
        // Reset the MAC state. Session and pending data transfers will be discarded.
        LMIC_reset();

        // Set static session parameters. Instead of dynamically establishing a session
        // by joining the network, precomputed session parameters are be provided.
        #ifdef PROGMEM
        // On AVR, these values are stored in flash and only copied to RAM
        // once. Copy them to a temporary buffer here, LMIC_setSession will
        // copy them into a buffer of its own again.
        uint8_t appskey[sizeof(APPSKEY)];
        uint8_t nwkskey[sizeof(NWKSKEY)];
        memcpy_P(appskey, APPSKEY, sizeof(APPSKEY));
        memcpy_P(nwkskey, NWKSKEY, sizeof(NWKSKEY));
        LMIC_setSession (0x1, DEVADDR, nwkskey, appskey);
        #else
        // If not running an AVR with PROGMEM, just use the arrays directly
        LMIC_setSession (0x1, DEVADDR, NWKSKEY, APPSKEY);
        #endif

        #if defined(CFG_eu868)
        // Set up the channels used by the Things Network, which corresponds
        // to the defaults of most gateways. Without this, only three base
        // channels from the LoRaWAN specification are used, which certainly
        // works, so it is good for debugging, but can overload those
        // frequencies, so be sure to configure the full frequency range of
        // your network here (unless your network autoconfigures them).
        // Setting up channels should happen after LMIC_setSession, as that
        // configures the minimal channel set.
        // NA-US channels 0-71 are configured automatically
        LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
        LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
        LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
        LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
        LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
        LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
        LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
        LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
        LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
        // TTN defines an additional channel at 869.525Mhz using SF9 for class B
        // devices' ping slots. LMIC does not have an easy way to define set this
        // frequency and support for class B is spotty and untested, so this
        // frequency is not configured here.
        #elif defined(CFG_us915)
        // NA-US channels 0-71 are configured automatically
        // but only one group of 8 should (a subband) should be active
        // TTN recommends the second sub band, 1 in a zero based count.
        // https://github.com/TheThingsNetwork/gateway-conf/blob/master/US-global_conf.json
        LMIC_selectSubBand(1);
        #endif

        // Disable link check validation
        LMIC_setLinkCheckMode(0);

        // TTN uses SF9 for its RX2 window.
        LMIC.dn2Dr = DR_SF9;

        // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
        LMIC_setDrTxpow(DR_SF7,14);

        // Start job
        do_send(&sendjob);
}

void loop () {
        os_runloop_once();
}
