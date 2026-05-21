/******************************************************
*  Elecraft T1 FTX-1F Interface by KW9D               *
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
#define debug


struct bandTable {
  const int     band;      // T1 band code (1-11)
  const String  bandText;
  const uint8_t abcd;      // FTX-1F BAND DATA: bit3=A, bit2=B, bit1=C, bit0=D
};
/*                  T1 code, Band,   ABCD (from FTX-1F TUNER/LINEAR band data table) */
struct bandTable bandTableList[] = {
  {12, "2m",   0b1101},  // 144 MHz (debug only)
  {13, "70cm", 0b0011},  // 430 MHz (debug only)  A=H B=H C=L D=L
  { 1, "160m", 0b1000},  // 1.8 MHz  A=H B=L C=L D=L
  { 2, "80m",  0b0100},  // 3.5 MHz  A=L B=H C=L D=L
  { 4, "40m",  0b1100},  // 5/7 MHz  A=L B=L C=H D=H  (FTX-1F shares 5 & 7 MHz)
  { 5, "30m",  0b0010},  // 10 MHz   A=L B=L C=H D=L (observed)
  { 6, "20m",  0b1010},  // 14 MHz   A=H B=L C=H D=L
  { 7, "17m",  0b0110},  // 18 MHz   A=L B=H C=H D=L
  { 8, "15m",  0b1110},  // 21 MHz   A=H B=H C=H D=L
  { 9, "12m",  0b0001},  // 24.5 MHz A=H B=L C=L D=L 
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
  Serial.println("Pin assignments:");
  Serial.print("  BAND_A -> "); Serial.println(BAND_A);
  Serial.print("  BAND_B -> "); Serial.println(BAND_B);
  Serial.print("  BAND_C -> "); Serial.println(BAND_C);
  Serial.print("  BAND_D -> "); Serial.println(BAND_D);
  Serial.print("  TUNE   -> "); Serial.println(TUNE);
  Serial.print("  DATA   -> "); Serial.println(DATA);
  Serial.println();
  Serial.println("Band Table:");
  Serial.println("+-------+-----+-------+-------+");
  Serial.println("| abcd  | dec | Band  | T1 ID |");
  Serial.println("+-------+-----+-------+-------+");
  for (int i = 0; i < (int)(sizeof(bandTableList) / sizeof(bandTable)); i++) {
    char binbuf[5];
    for (int b = 3; b >= 0; b--) binbuf[3-b] = ((bandTableList[i].abcd >> b) & 1) ? '1' : '0';
    binbuf[4] = '\0';
    char line[50];
    sprintf(line, "| %s  | %3d | %-5s |  %3d  |", binbuf, bandTableList[i].abcd, bandTableList[i].bandText.c_str(), bandTableList[i].band);
    Serial.println(line);
  }
  Serial.println("+-------+-----+-------+-------+\n");
  // Print observation table header
  Serial.println("A B C D  abcd(bin)  abcd(dec)  A B C D BAND DATA   BAND");
  Serial.println("------------------------------------------------------------");
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

    // Read pin values directly
    int a = digitalRead(BAND_A); // No inversion
    int b = digitalRead(BAND_B);
    int c = digitalRead(BAND_C);
    int d = digitalRead(BAND_D);
    uint8_t abcd = (d << 3) | (c << 2) | (b << 1) | a;

    // Determine band first
    band = determineFTX1FBand();
    static int lastValidBand = -1;
    if (band >= 1 && band <= 11) {
      lastValidBand = band;
    }

    // Handle serial commands
    if (Serial.available()) {
      char cmd = tolower(Serial.read());
      if (cmd == 'h') {
        Serial.println("\n--- CONSOLE COMMANDS ---");
        Serial.println("h - Display this help message");
        Serial.println("p - Print table headers");
        Serial.println("r - Resend current T1 band data");
        Serial.println("x - Reset all T1 relays (send 0000)");
        Serial.println();
      } else if (cmd == 'p') {
        Serial.println("\nA B C D  abcd(bin)  abcd(dec)  A B C D BAND DATA   BAND");
        Serial.println("------------------------------------------------------------\n");
      } else if (cmd == 'r') {
        if (lastValidBand >= 1 && lastValidBand <= 11) {
          Serial.println("[Console] Resending T1 band data...");
          setElecraftBand(lastValidBand);
        } else {
          Serial.println("[Console] No valid band to resend");
        }
      } else if (cmd == 'x') {
        Serial.println("[Console] Resetting all T1 relays (sending 0000)...");
        setElecraftBand(0);
      }
    }

    char binbuf[5];
    for (int b = 3; b >= 0; b--) binbuf[3-b] = ((abcd >> b) & 1) ? '1' : '0';
    binbuf[4] = '\0';

    char bandDataStr[16];
    sprintf(bandDataStr, "%c %c %c %c",
            a ? 'H' : 'L', b ? 'H' : 'L', c ? 'H' : 'L', d ? 'H' : 'L');

    const char* bandName = "no match";
    for (int i = 0; i < (int)(sizeof(bandTableList) / sizeof(bandTable)); i++) {
      if (abcd == bandTableList[i].abcd) {
        bandName = bandTableList[i].bandText.c_str();
        break;
      }
    }

    char line[120];
    sprintf(line, "%d %d %d %d  %-9s  %-9u  %-17s   %s",
            a, b, c, d, binbuf, (unsigned)abcd, bandDataStr, bandName);
    Serial.println(line);

    if ((band != -1) && (band != prevBand)) {
      // Only send to T1 for supported bands (1-11)
      if (band >= 1 && band <= 11) {
        Serial.println("Band change detected -> sending to T1");
        setElecraftBand(band);
      } else {
        Serial.println("Band change detected -> not supported by T1 (debug only)");
      }
      prevBand = band;
    }

    delay(1000); // Use delay for consistent serial output
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
  uint8_t abcd = (d << 3) | (c << 2) | (b << 1) | a;

  delay(10);

  uint8_t abcd2 = ((uint8_t)digitalRead(BAND_D) << 3) |
                  ((uint8_t)digitalRead(BAND_C) << 2) |
                  ((uint8_t)digitalRead(BAND_B) << 1) |
                  ((uint8_t)digitalRead(BAND_A) << 0);

  if (abcd != abcd2) {
    return -1;
  }

  for (int i = 0; i < (int)(sizeof(bandTableList) / sizeof(bandTable)); i++) {
    if (abcd == bandTableList[i].abcd) {
      return bandTableList[i].band;
    }
  }
  return -1;
}

/*------------------------------------------------------------------ */
void setElecraftBand(int band) {

#ifdef debug
  Serial.println("  [T1] Asserting TUNE for 500ms...");
#endif

  // Pull the TUNE line LOW (via NPN transistor) for 500ms
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