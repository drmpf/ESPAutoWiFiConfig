/**
   ESPAutoWiFiConfig.cpp
   by Matthew Ford,  2022/04/07
   See the tutorial at https://www.forward.com.au/pfod/ESPAutoWiFiCofig/index.html

   This file needs the SafeString library.  Install from the Arduino library manager.

   (c)2022 Forward Computing and Control Pty. Ltd.
   NSW, Australia  www.forward.com.au
   This code may be freely used for both private and commerical use.
   Provide this copyright is maintained.
*/

#include "ESPAutoWiFiConfig.h"
#include <EEPROM.h>

#ifdef ESP_PLATFORM   // ESP32
#include <WiFi.h>
#include <WebServer.h>
#else  // ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

#include <DNSServer.h>
#include <WiFiClient.h>
// these three includes are from the SafeString library.
// https://www.forward.com.au/pfod/ArduinoProgramming/SafeString/index.html
#include <millisDelay.h>
#include <SafeString.h>
#include <PinFlasher.h>
#include <PinFlasher.h>
#include <ESP32_WS2812Flasher.h>  // can include this for ESP8266 as well see the #ifdefs in the code

// normally DEBUG is commented out
#define DEBUG
static Stream* debugPtr = NULL;  // local to this file

// =================== ESP Access Point settings ===================
// this is the ip of the WiFi setup Access Point
static  IPAddress accessPoint_ip = IPAddress(192, 168, 1, 1);
// other choices
//static  IPAddress accessPoint_ip = IPAddress(10, 1, 1, 1);
//static  IPAddress accessPoint_ip = IPAddress(172, 16, 1, 1);

static char wifiWebConfigAP[] = "WiFiConfig";
static char wifiWebConfigPASSWORD[] = "12345678";


// ===================== DNS settings ==============================
// when WiFi config uses a static IP need need to manually set the dns servers
// these are the GOOGLE public DNS servers
static IPAddress dns1(8, 8, 8, 8);
static IPAddress dns2(8, 8, 8, 4);

// ============== EEPROM addresses used  =======================
static const int MAX_SSID_LEN_CONFIG = 32;
static const int MAX_PASSWORD_LEN_CONFIG = 64;
static const int MAX_STATICIP_LEN_CONFIG = 40;

struct WiFi_CONFIG_storage_struct {
  char ssid[MAX_SSID_LEN_CONFIG + 1]; // WIFI ssid + null
  char password[MAX_PASSWORD_LEN_CONFIG + 1]; // WiFi password,  if empyt use OPEN, else use AUTO (WEP/WPA/WPA2) + null
  char staticIP[MAX_STATICIP_LEN_CONFIG + 1]; // staticIP, if empty use DHCP + null
};

static const size_t wifiConfigFileAddress = 0;  // binary data , added to eepromOffset
static size_t rebootDetectionFileAddress; // sizeof(WiFi_CONFIG_storage_struct) rounded up, added to eepromOffset
static const uint8_t REBOOT_ACTIVE = 21;   // 0b010101
static const uint8_t REBOOT_INACTIVE = 42; // 0b101010

//======== EEPROM methods ========
static size_t eepromOffset = 0; // default
static void startEEPROM(); // start 512 size eeprom way  more then is needed
static bool checkValidRebootFlag();
static void writeRebootFlag();
static void clearRebootFlag();
static bool rebootFlagExists();
static void deleteWiFiConfigurationSettings();

static unsigned long SLOW_FLASH_WHILE_CONNECTING = 1150; // ms
static unsigned long FAST_FLASH_WHILE_AP_ACTIVE = 150; // ms on/off i.e. 150ms halfPeriod =>  3.33hz
// should avoid freq of 15 to 25hz (i.e. 33ms onoff to 20ms onoff) due to possibility of epileptic seizures https://en.wikipedia.org/wiki/Reflex_seizure
static bool inConfigMode = false;

static millisDelay endConfigTimer; // in config for 5min and nothing happening
static const unsigned long END_CONFIG_MS = 5ul * 60 * 1000; // 5mins to make first connection and show wifi config webpage
static millisDelay restartTimer; // config complete restart after 30sec
static const unsigned long RESTART_AFTER_CONFIG_MS = 30ul * 1000; // 30 sec after config set

static millisDelay doubleRebootTimer; // timer for double reboot, two reboots within 10sec starts config AccessPoint
static const unsigned long DOUBLE_REBOOT_MS = 10UL * 1000; // 10 sec

static millisDelay lostConnectionRebootTimer; // timer for when connect to AP lost, after 30sec reboot to clear state and try from scratch again
static const unsigned long LOST_CONNECTION_REBOOT_MS = 30UL * 1000; // 10 sec

static struct WiFi_CONFIG_storage_struct storage; // the gobal wifi config

static void saveWiFiConfig(); // returns pointer to wifi config storage or default values (if any)
static void loadWiFiConfig(); // returns pointer to wifi config storage or default values (if any)
static void printWifConfig(Stream *out);

static bool tryToConnectToConfiguredWiFiNetwork();
static void setupAP();

// save colours here until ESPAutoWiFiConfigSetup called
static uint8_t red = 0;
static uint8_t green = 128;
static uint8_t blue = 0;

static PinFlasher *flasherPtr = NULL;
static ESP32_WS2812Flasher *ws2812FlasherPtr = NULL; // only not NULL if ESP32 and -ve pin set
static const byte DNS_PORT = 53;
static DNSServer dnsServer;

#ifdef ESP_PLATFORM
static WebServer accessPointWebServer(80);  // this just sets portNo nothing else happens until begin() is called
#else
static ESP8266WebServer accessPointWebServer(80);  // this just sets portNo nothing else happens until begin() is called
#endif

// set these to your testing network if uploads clear out previous settings and return true
bool defaultNetworkSettings() {
	return false; // no defaults
	// example settings
//  cSFA(sfSSID, storage.ssid);
//  cSFA(sfPW, storage.password);
//  cSFA(sfIP, storage.staticIP);
//  sfSSID = "myWiFiSSID"
//  sfPW = "myWiFiPassword";
//  sfIP = "10.1.1.76"; // the static IP
//  return true;
}
	
/**
    call this from near the top of setup()
    use -ve pin number, e.g. -8 for ESP32C RGB_LED, to drive ws2812 RGB led on that pin, in which case highForLedOn is ignored
    for ESP32C3_RGB_LED use pinNo = -8
    for ESP32-S2-DevKitM-1   use pinNo = -18
    for ESP32-S2-Saola-1  use pinNo = -18
    for ESP32-S3-DevKitC-1  use pinNo = -48
  if (ESPAutoWiFiConfigSetup(13,true.0)) {
      return;
   }
   // rest of setup
*/
// if ESPAutoWiFiConfigSetup() returns true the in AP mode waiting for connection to set WiFi SSID/pw/ip settings
// ledPin is the output that drives the indicator led, 
// highForLedOn is true if +volts turns led on, else false if 0V out turns led on
// EEPROM_offset 0 unless you are using EEPROM in your code in which case pass in the size of the EEPROM your code uses
// and ESPAutoWiFiConfig will EEPROM addresses after that.
// Normally the led will turn OFF once the WiFi connects
// NOTE: if you want the led to stay ON, pass in the opposite value for highForLedOn, 
//  i.e.  highForLedOn = false true if +volts turns led on, highForLedOn = true if 0V out turns led on 
bool ESPAutoWiFiConfigSetup(int ledPin, bool highForLedOn, size_t EEPROM_offset) {
  bool outputInverted = !highForLedOn; // normal is highForOn
  eepromOffset = EEPROM_offset; 
  rebootDetectionFileAddress = (sizeof(WiFi_CONFIG_storage_struct) + 3) & (~3); // rounded up
  startEEPROM(); // uses eepromOffset and getESPAutoWiFiConfigEEPROM_Size()
  if (ledPin < 0) {
#if defined(ESP32)
    ledPin = -ledPin;
    ws2812FlasherPtr = new ESP32_WS2812Flasher(ledPin, outputInverted); // this will just be a PinFlasher for ESP8266 boards if -ve ledPin passed in
    flasherPtr = ws2812FlasherPtr;
    if (debugPtr) {
      debugPtr->println("Using ESP32_WS2812Flasher ");
    }
#else  // not ESP32 so ESP8266
    flasherPtr = new PinFlasher(ledPin, outputInverted);
    if (debugPtr) {
      debugPtr->println("Using PinFlasher ");
    }
#endif
  } else {
    flasherPtr = new PinFlasher(ledPin, outputInverted);
    if (debugPtr) {
      debugPtr->println("Using PinFlasher ");
    }
  }
  ESPAutoWiFiConfig_setColor(red, green, blue); // ignored if not ws2812 flasher
  flasherPtr->setOnOff(SLOW_FLASH_WHILE_CONNECTING); // slow flash while trying to connect

  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF); // force begin
  WiFi.setAutoConnect(false); // does not work for static ip see https://github.com/esp8266/Arduino/issues/2735
  WiFi.setAutoReconnect(true); // try to reconnect if we loose the connection
  if ((!checkValidRebootFlag()) || rebootFlagExists()) {
    // double reboot so start wifi config
    clearRebootFlag();
    setupAP(); // sets inConfigMode
    return true;
  }
  // else  continue and try to connect
  // create reboot file now
  if (debugPtr) {
    debugPtr->println("Create double reboot flag ");
  }
  writeRebootFlag();
  doubleRebootTimer.start(DOUBLE_REBOOT_MS);

  if (tryToConnectToConfiguredWiFiNetwork()) {
    if (debugPtr) {
      debugPtr->println(" connected to ");
      printWifConfig(debugPtr);
    }
    return false; // ok connected
  }

  // else
  if (debugPtr) {
    debugPtr->println(" did not connect to ");
    printWifConfig(debugPtr);
  }

  setupAP();
  return true; // in AP mode
}

/**
    call this from near the top of loop() with
  void loop() {
   if (ESPAutoWiFiConfigLoop()) {
      return;
   }
   // rest of loop
*/
// if ESPAutoWiFiConfigLoop() returns true the in AP mode processing setting WiFi SSID/pw/ip settings
bool ESPAutoWiFiConfigLoop() {
  if (!inConfigMode) {
    // check if should delete reboot file
    if (doubleRebootTimer.justFinished()) {
      // did not reboot in 10 sec so delete reboot file
      if (debugPtr) {
        debugPtr->println("Double reboot timed out clear flag ");
      }
      clearRebootFlag();
    }
    if (WiFi.status() != WL_CONNECTED) {
      flasherPtr->setOnOff(SLOW_FLASH_WHILE_CONNECTING); // ignored if already flashing at 100ms
      if (!lostConnectionRebootTimer.isRunning()) {
      	lostConnectionRebootTimer.start(LOST_CONNECTION_REBOOT_MS);
      }
    } else {// (WiFi.status() == WL_CONNECTED)
      // so all OK so turn led OFF
      lostConnectionRebootTimer.stop();
      flasherPtr->setOnOff(PIN_OFF); // ignored if already OFF
    }
    if(lostConnectionRebootTimer.justFinished()) {
    	// did not auto reconnect after 30 sec start from scratch again
    	clearRebootFlag(); // should not be needed but..
    	ESP.restart(); // see https://github.com/esp8266/Arduino/issues/1017  seems to work here
    }    	
    flasherPtr->update();
    return false; // not doing wifi config so just ignore this call
  }

  // else in config mode
  lostConnectionRebootTimer.stop();
  flasherPtr->update();
  dnsServer.processNextRequest();
  accessPointWebServer.handleClient();
  if (endConfigTimer.justFinished()) {
    ESP.restart(); // see https://github.com/esp8266/Arduino/issues/1017  seems to work here
  }
  if (restartTimer.justFinished()) {
    ESP.restart(); // see https://github.com/esp8266/Arduino/issues/1017  seems to work here
  }
  return true;
}

// set the RGB led colour on ESP32C OR other ESP32 board using ws2812 addressable RGB led on a GPIO pin
// ESP32C uses GPIO8 see ESP32C_RGB_LED above
/** Set RGB value for the WS2812 LED

   @param[in] red Intensity of Red color 0 to 255 (0-100%)
   @param[in] green Intensity of Green color 0 to 255 (0-100%)
   @param[in] blue Intensity of Green color 0 to 255 (0-100%)
*/
void ESPAutoWiFiConfig_setColor(uint8_t _red, uint8_t _green, uint8_t _blue) {
  red = _red;
  green = _green;
  blue = _blue;
  if (ws2812FlasherPtr) {
    ws2812FlasherPtr->setColor(red, green, blue);
  } // else ignore
}


// returns false if fails to connect in 30 sec;
static bool tryToConnectToConfiguredWiFiNetwork() {
  loadWiFiConfig();
  // ======================= connect to router ===================
  WiFi.mode(WIFI_STA);
  if (storage.staticIP[0] != '\0') {
    IPAddress ip;
    bool validIp = ip.fromString(storage.staticIP);
    if (validIp) {
      IPAddress gateway(ip[0], ip[1], ip[2], 1); // set gatway to ... 1
      IPAddress subnet_ip = IPAddress(255, 255, 255, 0);
      WiFi.config(ip, gateway, subnet_ip, dns1, dns2);
    } else {
      if (debugPtr) {
        debugPtr->print("Using DHCP, staticIP is invalid: "); debugPtr->println(storage.staticIP);
      }
    }
  } // else leave as DHCP

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(storage.ssid, storage.password);
    flasherPtr->setOnOff(SLOW_FLASH_WHILE_CONNECTING);
    if (debugPtr) {
      debugPtr->println("   Connecting to WiFi");
    }
  } else {
    flasherPtr->setOnOff(PIN_OFF);
    if (debugPtr) {
      debugPtr->println("   Already connected to WiFi");
    }
  }
  // Wait for connection for 30sec
  unsigned long pulseCounter = 0;
  unsigned long delay_ms = 100;
  unsigned long maxCount = (30 * 1000) / delay_ms; // delay below
  while ((WiFi.status() != WL_CONNECTED) && (pulseCounter < maxCount)) {
    pulseCounter++;
    delay(delay_ms); // short delay to call flasher.update() often also prevents WDT timing out
    flasherPtr->update();
    if (debugPtr) {
      if ((pulseCounter % 10) == 0) {
        debugPtr->print(".");
      }
    }
    // check if should delete reboot file
    if (doubleRebootTimer.justFinished()) {
      // did not reboot in 10 sec so delete reboot file
      if (debugPtr) {
        debugPtr->println("Double reboot timed out clear flag ");
      }
      clearRebootFlag();
    }
  }
  if (debugPtr) {
    debugPtr->println();
  }
  if (pulseCounter >= maxCount) {
    return false;
  } // else

  flasherPtr->setOnOff(PIN_OFF);
  if (debugPtr) {
    debugPtr->println("");
    debugPtr->print("Connected to ");
    debugPtr->println(storage.ssid);
    debugPtr->print("IP address: ");
    debugPtr->println(WiFi.localIP());
  }
  return true;
}

void printWifConfig(Stream *out) {
  if (out == NULL) {
    return;
  }
  out->print("ssid:");
  out->println(storage.ssid);
  out->print("password:");
  out->println(storage.password);
  out->print("staticIP:");
  out->println(storage.staticIP);
}




// =========================  Access Point Web server and pages ==================
static void handleNotFound();
static void handleRoot();
static void handleConfig();
String urlDecode(const String & text);
cSF(sfStrongestAP, MAX_SSID_LEN_CONFIG);




/**
   will return name of AP with strongest signal found or return empty string if none found
*/
static void scanForStrongestAP(SafeString & result) {
  result.clear();
  // WiFi.scanNetworks will return the number of networks found
  int8_t n = WiFi.scanNetworks();
  if (n <= 0) {
    if (debugPtr) {
      debugPtr->println("WiFi network scan failed");
    }
    return;
  }
  if (debugPtr) {
    debugPtr->println("Scan done");
    debugPtr->print("Found ");   debugPtr->print(n);    debugPtr->println(" networks");
  }
  int32_t maxRSSI = -10000;
  for (int8_t i = 0; i < n; ++i) {
    //const char * ssid_scan = WiFi.SSID_charPtr(i);
    int32_t rssi_scan = WiFi.RSSI(i);
    if (rssi_scan > maxRSSI) {
      maxRSSI = rssi_scan;
      String ssid = WiFi.SSID(i);
      result = ssid.c_str();
    }
    if (debugPtr) {
      debugPtr->print(result);
      debugPtr->print(" ");
      debugPtr->println(rssi_scan);
    }
    delay(0);
  }
}


/**
   sets up AP and loads current wifi settings
*/
static void setupAP() {
  clearRebootFlag(); // start normally next time
  loadWiFiConfig(); // sets global storage
  flasherPtr->setOnOff(FAST_FLASH_WHILE_AP_ACTIVE);
  /**
    Start scan WiFi networks available
    @param async         run in async mode
    @param show_hidden   show hidden networks
    @param channel       scan only this channel (0 for all channels)
    @param ssid*         scan for only this ssid (NULL for all ssid's)
    @return Number of discovered networks
  */
  inConfigMode = true; // in config mode
  if (debugPtr) {
    debugPtr->print(F("Setting up Access Point for ")); debugPtr->println(wifiWebConfigAP);
  }
  // connect to temporary wifi network for setup

  scanForStrongestAP(sfStrongestAP);
  if (debugPtr) {
    debugPtr->print(F("configure ")); debugPtr->println(wifiWebConfigAP);
  }

  if (debugPtr) {
    debugPtr->println(F("Access Point setup"));
  }

  WiFi.softAPConfig(accessPoint_ip, accessPoint_ip, IPAddress(255, 255, 255, 0)); // call this first!!
  WiFi.softAP(wifiWebConfigAP, wifiWebConfigPASSWORD);
  // if DNSServer is started with "*" for domain name, it will reply with
  // provided IP to all DNS request
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", accessPoint_ip);

  if (debugPtr) {
    debugPtr->println("done");
    IPAddress myIP = WiFi.softAPIP();
    debugPtr->print(F("AP IP address: "));
    debugPtr->println(myIP);
  }
  delay(10);
  accessPointWebServer.on ( "/", handleRoot );
  accessPointWebServer.on ( "/config", handleConfig );
  accessPointWebServer.onNotFound ( handleNotFound );
  accessPointWebServer.begin();
  if (debugPtr) {
    debugPtr->println ( "HTTP accessPointWebServer started" );
  }
  endConfigTimer.start(END_CONFIG_MS);
}

static void handleConfig() {
  restartTimer.start(RESTART_AFTER_CONFIG_MS); // restart in 30sec
  // set defaults
  if (accessPointWebServer.args() > 0) {
    if (debugPtr) {
      String message = "Config results\n\n";
      message += "URI: ";
      message += accessPointWebServer.uri();
      message += "\nMethod: ";
      message += ( accessPointWebServer.method() == HTTP_GET ) ? "GET" : "POST";
      message += "\nArguments: ";
      message += accessPointWebServer.args();
      message += "\n";
      for ( uint8_t i = 0; i < accessPointWebServer.args(); i++ ) {
        message += " " + accessPointWebServer.argName ( i ) + ": " + accessPointWebServer.arg ( i ) + "\n";
      }
      debugPtr->println(message);
      debugPtr->println();
    }

    cSFA(sfSSID, storage.ssid);
    cSFA(sfPW, storage.password);
    cSFA(sfIP, storage.staticIP);

    uint8_t numOfArgs = accessPointWebServer.args();
    uint8_t i = 0;
    for (; (i < numOfArgs); i++ ) {
      // check field numbers
      if (accessPointWebServer.argName(i)[0] == '1') {
        String decoded = urlDecode(accessPointWebServer.arg(i)); // result is always <= source so just copy over
        decoded.trim();
        sfSSID = decoded.c_str();
      } else if (accessPointWebServer.argName(i)[0] == '2') {
        String decoded = urlDecode(accessPointWebServer.arg(i)); // result is always <= source so just copy over
        decoded.trim();
        if (decoded != "*") {
          // update it
          sfPW = decoded.c_str();
        }
        // if password all blanks make it empty
      } else if (accessPointWebServer.argName(i)[0] == '3') {
        String decoded = urlDecode(accessPointWebServer.arg(i)); // result is always <= source so just copy over
        decoded.trim();
        sfIP = decoded.c_str();
      }
    }

    if (debugPtr) {
      debugPtr->println();
      printWifConfig(debugPtr);
    }

    // store the settings
    saveWiFiConfig();
  } // else if no args just return current settings

  delay(0);
  loadWiFiConfig();
  if (debugPtr) {
    debugPtr->println();
    printWifConfig(debugPtr);
  }
  String rtnMsg = "<html>"
                  "<head>"
                  "<title>Setup WiFi Network Configuration</title>"
                  "<meta charset=\"utf-8\" />"
                  "<meta name=viewport content=\"width=device-width, initial-scale=1\">"
                  "</head>"
                  "<body>"
                  "<h2>WiFi Network Configuration Settings saved.</h2>"
                  "Power cycle to connect to ";
  if (storage.password[0] == '\0') {
    rtnMsg += "the open network ";
  }
  rtnMsg += "<b>";
  rtnMsg += storage.ssid;
  rtnMsg += "</b>";

  if (storage.staticIP[0] == '\0') {
    rtnMsg += "<br> using DCHP to get its IP address";
  } else { // staticIP
    IPAddress ip;
    if (!ip.fromString(storage.staticIP)) {
      rtnMsg += "<br><br> IP address entered is invalid using DHCP</b>";
    } else {
      rtnMsg += "<br> using IP addess ";
      rtnMsg += "<b>";
      rtnMsg += storage.staticIP;
      rtnMsg += "</b>";
    }
  }
  rtnMsg += "<p>";
  rtnMsg += "<b>You also need to reconnect your mobile<br> to the ";
  rtnMsg += storage.ssid;
  rtnMsg += " network</b>";
  rtnMsg += "<p>";
  rtnMsg += " The device will auto restart in 30 seconds</b>";
  rtnMsg += "<p>";
  rtnMsg += " You can force the device back into WiFi configuration mode by turning the device off<br>";
  rtnMsg += " and then turning it on and off quickly,<br> i.e. within 10 sec";
  rtnMsg += "<p> Then the next time the device is turned on it will be in WiFi configuration mode.";
  rtnMsg += "</body>";
  rtnMsg += "</html>";

  accessPointWebServer.send ( 200, "text/html", rtnMsg );
}


static void handleRoot() {
  endConfigTimer.start(END_CONFIG_MS); // allow another 5mins stop 5min time out on first connection
  String msg;
  msg = "<html>"
        "<head>"
        "<title>Setup WiFi Network Configuration</title>"
        "<meta charset=\"utf-8\" />"
        "<meta name=viewport content=\"width=device-width, initial-scale=1\">"
        "</head>"
        "<body>"
        "<h2>Setup WiFi Network Configuration</h2>"
        "<p>Use this form to configure your device to connect to your WiFi network.<br>"
        "<i>Leading and trailing spaces are trimmed.</i></p>"
        "<form class=\"form\" method=\"post\" action=\"/config\" >"
        "<p class=\"name\">"
        "<label for=\"name\">Network SSID (strongest network found shown here)</label><br>"
        "<input type=\"text\" name=\"1\" id=\"ssid\" placeholder=\"wifi network name\"  required "; // field 1

  if (!sfStrongestAP.isEmpty()) {
    msg += " value=\"";
    msg += sfStrongestAP.c_str();
    msg += "\" ";
  }
  msg += " />"
         "<p class=\"password\">"
         "<label for=\"password\">Password for WEP/WPA/WPA2 (enter a space if there is no password, i.e. OPEN)<br>"
         "To use existing password leave as * </label><br>"
         "<input type=\"text\" name=\"2\" id=\"password\" placeholder=\"wifi network password\" autocomplete=\"off\" required "; // field 2
  if (storage.password[0] != '\0') {
    msg += " value=\"";
    msg += "*"; //storage.password[0];
    msg += "\" ";
  }
  msg += " />"
         "</p>"
         "<p class=\"static_ip\">"
         "<label for=\"static_ip\">Set the Static IP for this device</label><br>"
         "(If this field is empty, DHCP will be used to get an IP address)<br>"
         "<input type=\"text\" name=\"3\" id=\"static_ip\" placeholder=\"192.168.4.99\" minlength=\"7\" maxlength=\"15\" size=\"15\"  "  // field 3
         " pattern=\"\\b(?:(?:25[0-4]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-4]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b\"";
  if (storage.staticIP[0] != '\0') {
    msg += " value=\"";
    msg += storage.staticIP;
    msg += "\" ";
  }
  msg += " />"
         "</p>"
         "<p class=\"submit\">"
         "<input type=\"submit\" style=\"font-size:25px;\" value=\"Configure\"  />"
         "</p>"
         "</form>"
         "The device will auto-restart in 5 mins if current configuration is not changed"
         "</body>"
         "</html>";

  accessPointWebServer.send ( 200, "text/html", msg );
}


static void handleNotFound() {
  handleRoot();
}


String urlDecode(const String & text) {
  String decoded;
  char temp[] = "0x00";
  unsigned int len = text.length();
  unsigned int i = 0;
  while (i < len)
  {
    char decodedChar;
    char encodedChar = text.charAt(i++);
    if ((encodedChar == '%') && (i + 1 < len))
    {
      temp[2] = text.charAt(i++);
      temp[3] = text.charAt(i++);

      decodedChar = strtol(temp, NULL, 16);
    }
    else {
      if (encodedChar == '+')
      {
        decodedChar = ' ';
      }
      else {
        decodedChar = encodedChar;  // normal ascii char
      }
    }
    decoded += decodedChar;
  }
  return decoded;
}

// ==============  set debug out ============
void setESPAutoWiFiConfigDebugOut(Stream &out) {
  debugPtr = &out;
}

// =============== EEPROM methods ====================
size_t getESPAutoWiFiConfigEEPROM_Size() {
  size_t rebootDetectionFileAddress = (sizeof(WiFi_CONFIG_storage_struct) + 3) & (~3); // rounded up
  size_t configSize = rebootDetectionFileAddress + 1 ; // rounded up storage + byte for reboot
  configSize = (configSize +8 + 3) & (~3); // round up again
  return configSize;
}	

void startEEPROM() {
  EEPROM.begin(eepromOffset + getESPAutoWiFiConfigEEPROM_Size());
  if (!checkValidRebootFlag()) {
    if (debugPtr) {
      debugPtr->println("EEPROM invalid clear wificonfig");
    }
    printWifConfig(debugPtr);
    deleteWiFiConfigurationSettings();
  }
}

static bool checkValidRebootFlag() {
  uint8_t flag = EEPROM.read(rebootDetectionFileAddress + eepromOffset);
  if ((flag == REBOOT_ACTIVE) || (flag == REBOOT_INACTIVE)) {
    return true;
  }
  // else
  return defaultNetworkSettings(); // false if none
}

static void writeRebootFlag() {
  EEPROM.write(rebootDetectionFileAddress + eepromOffset, REBOOT_ACTIVE);
  EEPROM.commit();
}

static void clearRebootFlag() {
  EEPROM.write(rebootDetectionFileAddress + eepromOffset, REBOOT_INACTIVE);
  EEPROM.commit();
}

static bool rebootFlagExists() {
  uint8_t flag = EEPROM.read(rebootDetectionFileAddress + eepromOffset);
  if (flag == REBOOT_ACTIVE) {
    return true;
  }
  // else
  return false;
}

static void saveWiFiConfig() {
  EEPROM.put(wifiConfigFileAddress + eepromOffset, storage);
  EEPROM.commit();
  if (debugPtr) {
    debugPtr->println("saved config");
    printWifConfig(debugPtr);
  }
}

static void loadWiFiConfig() {
  EEPROM.get(wifiConfigFileAddress + eepromOffset, storage);
  // clean it up cSFA makes sure there is terminating '\0'
  cSFA(sfSSID, storage.ssid);
  cSFA(sfPW, storage.password);
  cSFA(sfIP, storage.staticIP);
  if (debugPtr) {
    debugPtr->println("loaded config");
    printWifConfig(debugPtr);
  }
}

// this usually only used for software testing
static void deleteWiFiConfigurationSettings() {
  for (size_t i = 0; i < sizeof(storage.ssid); i++) {
    storage.ssid[i] = '\0'; // clear it
  }
  for (size_t i = 0; i < sizeof(storage.password); i++) {
    storage.password[i] = '\0'; // clear it
  }
  for (size_t i = 0; i < sizeof(storage.staticIP); i++) {
    storage.staticIP[i] = '\0'; // clear it
  }
  saveWiFiConfig();
}
