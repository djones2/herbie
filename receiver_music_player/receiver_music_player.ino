// Specifically for use with the Adafruit Feather, the pins are pre-set here!

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>
#include <RH_RF69.h>
#include <RHReliableDatagram.h>

// These are the pins used
#define VS1053_RESET   -1     // VS1053 reset pin (not used!)

#define VS1053_CS       6     // VS1053 chip select pin (output)
#define VS1053_DCS     10     // VS1053 Data/command select pin (output)
#define CARDCS          5     // Card chip select pin
// DREQ should be an Int pin *if possible* (not possible on 32u4)
#define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin

// Change to 434.0 or other frequency, must match RX's freq!
#define RF69_FREQ 915.0

// who am i? (server address)
#define MY_ADDRESS     1

#define RFM69_CS      8
#define RFM69_INT     3
#define RFM69_RST     4
#define LED           13

// Singleton instance of the radio driver
RH_RF69 rf69(RFM69_CS, RFM69_INT);

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram rf69_manager(rf69, MY_ADDRESS);

int16_t packetnum = 0;  // packet counter, we increment per xmission

Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

 void printDirectory(File dir, int numTabs);

void setup() {
  Serial.begin(115200);

  // if you're using Bluefruit or LoRa/RFM Feather, disable the radio module
  pinMode(8, INPUT_PULLUP);

  pinMode(LED, OUTPUT);     
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, LOW);

  // Wait for serial port to be opened, remove this line for 'standalone' operation
  while (!Serial) { delay(1); }
  delay(500);
  Serial.println("\n\nAdafruit VS1053 Feather Test");
  Serial.println("Feather Addressed RFM69 RX Test!");
  Serial.println();

    // manual reset
  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);

  if (!rf69_manager.init()) {
    Serial.println("RFM69 radio init failed");
    while (1);
  }
  Serial.println("RFM69 radio init OK!");
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM (for low power module)
  // No encryption
  if (!rf69.setFrequency(RF69_FREQ)) {
    Serial.println("setFrequency failed");
  }
  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }

  Serial.println(F("VS1053 found"));
 
  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
  
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");

  // If you are using a high power RF69 eg RFM69HW, you *must* set a Tx power with the
  // ishighpowermodule flag set like this:
  rf69.setTxPower(20, true);  // range from 14-20 for power, 2nd arg must be true for 69HCW

  // The encryption key has to be the same as the one in the server
  uint8_t key[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
  rf69.setEncryptionKey(key);
  
  pinMode(LED, OUTPUT);

  Serial.print("RFM69 radio @");  Serial.print((int)RF69_FREQ);  Serial.println(" MHz");
  
  // list files
  //printDirectory(SD.open("/"), 0);
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(10,10);
  
#if defined(__AVR_ATmega32U4__) 
  // Timer interrupts are not suggested, better to use DREQ interrupt!
  // but we don't have them on the 32u4 feather...
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int
#else
  // If DREQ is on an interrupt pin we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
#endif
  


//  Serial.println(F("Playing track 002"));
//  musicPlayer.startPlayingFile("/track002.mp3");
}

// Dont put this on the stack:
uint8_t data[] = "HELLO";
// Dont put this on the stack:
uint8_t buf[RH_RF69_MAX_MESSAGE_LEN];


void loop() {
  //Serial.print("IN MAIN"); 

  if (rf69_manager.available())
  {
    musicPlayer.playFullFile("/track001.mp3");
    Serial.print("Received signal, playing song.");
    delay(1000000000000); // length of song
    // Wait for a message addressed to us from the client
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (rf69_manager.recvfromAck(buf, &len, &from)) {
      buf[len] = 0; // zero out remaining string
      
      Serial.print("Got packet from #"); Serial.print(from);
      Serial.print(" [RSSI :");
      Serial.print(rf69.lastRssi());
      Serial.print("] : ");
      Serial.println((char*)buf);
      Blink(LED, 40, 3); //blink LED 3 times, 40ms between blinks

      // Play a file in the background, REQUIRES interrupts!
      Serial.println(F("Playing full track 001"));
      //musicPlayer.playFullFile("/track001.mp3");
      Serial.print(".");
      // File is playing in the background
      if (musicPlayer.stopped()) {
        Serial.println("Done playing music");
      while (1) {
          delay(10);  // we're done! do nothing...
       }
      }
      if (Serial.available()) {
        char c = Serial.read();
        
        // if we get an 's' on the serial console, stop!
        if (c == 's') {
          musicPlayer.stopPlaying();
        }
        
        // if we get an 'p' on the serial console, pause/unpause!
        if (c == 'p') {
          if (! musicPlayer.paused()) {
            Serial.println("Paused");
            musicPlayer.pausePlaying(true);
          } else { 
            Serial.println("Resumed");
            musicPlayer.pausePlaying(false);
          }
        }
      }

      // Send a reply back to the originator client
      if (!rf69_manager.sendtoWait(data, sizeof(data), from))
        Serial.println("Sending failed (no ack)");
    }
    delay(100);
  }
  delay(100);
}



/// File listing helper
void printDirectory(File dir, int numTabs) {
   while(true) {
     
     File entry =  dir.openNextFile();
     if (! entry) {
       // no more files
       //Serial.println("**nomorefiles**");
       break;
     }
     for (uint8_t i=0; i<numTabs; i++) {
       Serial.print('\t');
     }
     Serial.print(entry.name());
     if (entry.isDirectory()) {
       Serial.println("/");
       printDirectory(entry, numTabs+1);
     } else {
       // files have sizes, directories do not
       Serial.print("\t\t");
       Serial.println(entry.size(), DEC);
     }
     entry.close();
   }
}

void Blink(byte PIN, byte DELAY_MS, byte loops) {
  for (byte i=0; i<loops; i++)  {
    digitalWrite(PIN,HIGH);
    delay(DELAY_MS);
    digitalWrite(PIN,LOW);
    delay(DELAY_MS);
  }
}
