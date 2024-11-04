#include "sml.h"
#include <Arduino.h>

#define MAX_STR_MANUF 5
#define BUFFER_SIZE 1024  // Adjust size as needed for data buffer

unsigned char manuf[MAX_STR_MANUF];
double T1Wh = -2, SumWh = -2, Watt = -2;

typedef struct {
  const unsigned char OBIS[6];
  void (*Handler)();
} OBISHandler;

void Manufacturer() { smlOBISManufacturer(manuf, MAX_STR_MANUF); }

void PowerT1() { smlOBISWh(T1Wh); }

void PowerSum() { smlOBISWh(SumWh); }

void PowerW() { smlOBISW(Watt); }

void processBuffer(unsigned char* buffer, int length);

// clang-format off
OBISHandler OBISHandlers[] = {
  {{ 0x81, 0x81, 0xc7, 0x82, 0x03, 0xff }, &Manufacturer}, /* 129-129:199.130.3*255 */
  {{ 0x01, 0x00, 0x01, 0x08, 0x01, 0xff }, &PowerT1},      /*   1-  0:  1.  8.1*255 (T1) */
  {{ 0x01, 0x00, 0x01, 0x08, 0x00, 0xff }, &PowerSum},     /*   1-  0:  1.  8.0*255 (T1 + T2) */
  {{ 0x01, 0x00, 0x0F, 0x07, 0x00, 0xff }, &PowerW},       /*   1-  0: 15.  7.0*255 (Watt) */
  {{ 0, 0 }}
};
// clang-format on

unsigned char buffer[BUFFER_SIZE];
int bufferIndex = 0;

void setup() {
  Serial.begin(115200); // Debugging Serial
  Serial1.begin(9600, SERIAL_8N1, 3, 1); // Initialize Serial2 for reading from pins 16 (RX) and 17 (TX)
  
  Serial.println("Connected to Serial1:");
  delay(1000); // Optional delay to allow serial connection to establish
}

void loop() {
  // Check if data is available on Serial2
  //Serial1.write(0x00);

  while (Serial1.available() > 0) {
    // Read a byte from Serial2
    
    unsigned char c = Serial1.read();

    // Store in buffer if there's room
    if (bufferIndex < BUFFER_SIZE) {
      buffer[bufferIndex++] = c;
    } else {
      // Reset buffer if it's full
      bufferIndex = 0;
    }

    // Process buffer when end of message is likely detected (for example by a specific byte or after a set amount of data)
    if (bufferIndex > 0) {
      processBuffer(buffer, bufferIndex);
      bufferIndex = 0; // Reset buffer after processing
    }
  }
}

void processBuffer(unsigned char* buffer, int length) {
  sml_states_t s;
  int iHandler = 0;

  for (int i = 0; i < length; ++i) {
    unsigned char c = buffer[i];
    s = smlState(c);

    if (s == SML_START) {
      // Reset local vars
      manuf[0] = 0;
      T1Wh = -3;
      SumWh = -3;
    }

    if (s == SML_LISTEND) {
      // Check handlers on last received list
      for (iHandler = 0; OBISHandlers[iHandler].Handler != 0 &&
                         !(smlOBISCheck(OBISHandlers[iHandler].OBIS));
           iHandler++)
        ;
      if (OBISHandlers[iHandler].Handler != 0) {
        OBISHandlers[iHandler].Handler();
      }
    }

    if (s == SML_UNEXPECTED) {
      Serial.printf(">>> Unexpected byte >%02X<! <<<\n", c);
    }

    if (s == SML_FINAL) {
      Serial.println(">>> FINAL! Checksum OK");
      Serial.printf(">>> Manufacturer.............: %s\n", manuf);
      Serial.printf(">>> Power T1    (1-0:1.8.1)..: %.3f Wh\n", T1Wh);
      Serial.printf(">>> Power T1+T2 (1-0:1.8.0)..: %.3f Wh\n", SumWh);
      Serial.printf(">>> Watt        (1-0:15.7.0).: %.3f W\n\n", Watt);
    }
  }
}
