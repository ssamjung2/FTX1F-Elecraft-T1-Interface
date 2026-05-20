/******************************************************
*  Elecraft T1 FTX-1F Interface                       *
*                                                     *
*  This Arduino sketch allows the Yaesu FTX-1F to     *
*  control an Elecraft T1 ATU via the T1's remote     *
*  control interface.                                 *
*                                                     *
*  Reads the 4-bit digital BAND DATA (A/B/C/D) from   *
*  the FTX-1F TUNER/LINEAR connector and sends the    *
*  corresponding band code to the T1.                 *
*                                                     *
*  Based on work by Gareth - GI1MIC, 2017             *
*  Based on work by Matthew Robinson - VK6MR, 2011    *
*  See http://blog.zensunni.org/                      *
*                                                     * 
*  This code is released under a Creative Commons     *
*  Attribution-NonCommercial-ShareAlike 3.0           *
*  Unported (CC BY-NC-SA 3.0) license.                *
*                                                     *
*  http://creativecommons.org/licenses/by-nc-sa/3.0/  *
*                                                     *
******************************************************/

#include <Adafruit_SleepyDog.h>

// Comment out the following to enable degugging and 
// disable power saving (which clashes with the USB interface)
// #define debug


struct bandTable {
  const int     band;      // T1 band code (1-11)
  const String  bandText;
  const uint8_t abcd;      // FTX-1F BAND DATA: bit3=A, bit2=B, bit1=C, bit0=D
};
/*                  T1 code, Band,   ABCD (from FTX-1F TUNER/LINEAR band data table) */
struct bandTable bandTableList[] = {
  { 1, "160m", 0b1000},  // 1.8 MHz  A=H B=L C=L D=L
  { 2, "80m",  0b0100},  // 3.5 MHz  A=L B=H C=L D=L
  { 4, "40m",  0b1100},  // 5/7 MHz  A=H B=H C=L D=L  (FTX-1F shares 5 & 7 MHz)
  { 5, "30m",  0b0011},  // 10 MHz   A=L B=L C=H D=H
  { 6, "20m",  0b1010},  // 14 MHz   A=H B=L C=H D=L
  { 7, "17m",  0b0110},  // 18 MHz   A=L B=H C=H D=L
  { 8, "15m",  0b1110},  // 21 MHz   A=H B=H C=H D=L
  { 9, "12m",  0b0001},  // 24.5 MHz A=L B=L C=L D=H
  {10, "10m",  0b1001},  // 28 MHz   A=H B=L C=L D=H
  {11, "6m",   0b0101},  // 50 MHz   A=L B=H C=L D=H
  // 70/144/430 MHz omitted - T1 cannot tune these bands
};

// FTX-1F TUNER/LINEAR connector band data pins (active outputs from radio)
// Connect TX D(BAND A), RX D(BAND B), EXTS(DET)(BAND C), TRST(BAND D)
const int BAND_A = A0;
const int BAND_B = A1;
const int BAND_C = A2;
const int BAND_D = A3;

const int TUNE = 4;  // Ring of Elecraft T1 3.5mm jack via 1K and NPN transistor
const int DATA = 3;  // Tip of Elecraft T1 3.5mm jack

// Max wait for DATA line state transitions (ms)
const unsigned long DATA_TIMEOUT_MS = 200;

int prevBand = -1;

/*------------------------------------------------------------------ */
void setup() {
#ifdef debug
  Serial.begin(9600);
  while (!Serial) ; // wait for Arduino Serial Monitor (native USB boards)
  Serial.println("FTX-1F to Elecraft T1 interface");
  Serial.println("  BAND_A -> A0  BAND_B -> A1  BAND_C -> A2  BAND_D -> A3");
  Serial.println("  TUNE   -> D4  DATA   -> D3");
#endif

  pinMode(BAND_A, INPUT_PULLUP);
  pinMode(BAND_B, INPUT_PULLUP);
  pinMode(BAND_C, INPUT_PULLUP);
  pinMode(BAND_D, INPUT_PULLUP);

  pinMode(TUNE, OUTPUT);
  digitalWrite(TUNE, LOW);

  pinMode(DATA, INPUT);
}

/*------------------------------------------------------------------ */
void loop() {
      int band;

      band = determineFTX1FBand();

    if ((band != -1) && (band != prevBand)) {
      #ifdef debug
          Serial.println("Band change detected -> sending to T1");
      #endif
      setElecraftBand(band);
      prevBand = band;
    }
#ifdef debug
      delay(2000);
#else
      // Low power mode impacts the UART so is not used in debug mode
      Watchdog.sleep(2000);
#endif     
 }

/*------------------------------------------------------------------ */

int determineFTX1FBand() {

  // Read the 4-bit BAND DATA from the FTX-1F TUNER/LINEAR connector.
  // Double-read with 10ms gap: discard if the pins change between reads
  // (catches transients during band switches on the radio).
  uint8_t a = digitalRead(BAND_A);
  uint8_t b = digitalRead(BAND_B);
  uint8_t c = digitalRead(BAND_C);
  uint8_t d = digitalRead(BAND_D);
  uint8_t abcd = (a << 3) | (b << 2) | (c << 1) | d;

  delay(10);

  uint8_t abcd2 = ((uint8_t)digitalRead(BAND_A) << 3) |
                  ((uint8_t)digitalRead(BAND_B) << 2) |
                  ((uint8_t)digitalRead(BAND_C) << 1) |
                  ((uint8_t)digitalRead(BAND_D) << 0);

  if (abcd != abcd2) {
#ifdef debug
    Serial.println("BAND DATA  unstable - ignoring");
#endif
    return -1;
  }

#ifdef debug
  Serial.print("BAND DATA  A=");
  Serial.print(a ? "H" : "L");
  Serial.print(" B=");
  Serial.print(b ? "H" : "L");
  Serial.print(" C=");
  Serial.print(c ? "H" : "L");
  Serial.print(" D=");
  Serial.print(d ? "H" : "L");
  Serial.print("  (0b");
  Serial.print(abcd, BIN);
  Serial.print(")");
#endif

  for (int i = 0; i < (int)(sizeof(bandTableList) / sizeof(bandTable)); i++) {
    if (abcd == bandTableList[i].abcd) {
#ifdef debug
      Serial.println("  -> " + bandTableList[i].bandText);
#endif
      return bandTableList[i].band;
    }
  }
#ifdef debug
  Serial.println("  -> no match");
#endif
  return -1;
}

/*------------------------------------------------------------------ */
void setElecraftBand(int band) {

#ifdef debug
  Serial.println("  [T1] Asserting TUNE for 500ms...");
#endif

  // Pull the TUNE line high for half a second
  digitalWrite(TUNE, HIGH);
  delay(500);
  digitalWrite(TUNE, LOW);

  // The ATU will pull the DATA line HIGH for ~50ms; wait with timeout
#ifdef debug
  Serial.println("  [T1] Waiting for DATA to go HIGH...");
#endif
  unsigned long deadline = millis() + DATA_TIMEOUT_MS;
  while (digitalRead(DATA) == LOW  && millis() < deadline) {}
#ifdef debug
  if (millis() >= deadline) Serial.println("  [T1] WARNING: timeout waiting for DATA HIGH - T1 may not be responding");
#endif

#ifdef debug
  Serial.println("  [T1] Waiting for DATA to go LOW...");
#endif
  deadline = millis() + DATA_TIMEOUT_MS;
  while (digitalRead(DATA) == HIGH && millis() < deadline) {}
#ifdef debug
  if (millis() >= deadline) Serial.println("  [T1] WARNING: timeout waiting for DATA LOW - T1 handshake incomplete");
#endif

  // Wait 10ms
  delay(10);

  // and then send data on the DATA line
  pinMode(DATA, OUTPUT);

#ifdef debug
  Serial.print("  [T1] Sending bits: ");
  Serial.print((band >> 3) & 1);
  Serial.print(" ");
  Serial.print((band >> 2) & 1);
  Serial.print(" ");
  Serial.print((band >> 1) & 1);
  Serial.print(" ");
  Serial.println((band >> 0) & 1);
#endif

  // 1 bits are HIGH for 4ms
  // 0 bits are HIGH for 1.5ms
  // Gap between bits is 1.5ms LOW
  sendBit(band & 8);
  sendBit(band & 4);
  sendBit(band & 2);
  sendBit(band & 1);

  // Leave the line LOW and switch back to input
  digitalWrite(DATA, LOW);
  pinMode(DATA, INPUT);

#ifdef debug
  Serial.println("  [T1] Done");
#endif
}

/*------------------------------------------------------------------ */
void sendBit(int bit) {
  
  digitalWrite(DATA, HIGH);
  if (bit != 0) {
    delay(4);
  } else {
    delayMicroseconds(1500);
  }
  
  digitalWrite(DATA, LOW);
  delayMicroseconds(1500);
}