// Original code from: https://github.com/x893/CC.Flash
// Edited by Atc1441 Aaron Christophel : 07.10.2022 compatibility for ESP32

#include <ctype.h>


// All pins only bitbanged so choose the one you need
#define LED               22
#define CC_DC             19
#define CC_DD             23
#define CC_RST            33

#define BUFFER_SIZE       255

#define READ_TYPE_CODE    0
#define READ_TYPE_XDATA   1

byte inBuffer[BUFFER_SIZE];
byte inDataLen = 0;
byte idx;

void setup() {
  pinMode(CC_DC, OUTPUT);
  pinMode(CC_DD, OUTPUT);
  pinMode(CC_RST, OUTPUT);
  digitalWrite(CC_DC, LOW);
  digitalWrite(CC_DD, HIGH);
  digitalWrite(CC_RST, HIGH);

  Serial.begin(500000);
  LED_OFF();
  Serial.print("\r\n:");
}

void loop() {
  if (Serial.available() <= 0)
    return;

  byte inByte = Serial.read();
  if (inByte == '\r') {
    process_cmd();
    inDataLen = 0;
  } else if (inDataLen >= BUFFER_SIZE) {
    do {
      inByte = Serial.read();
    } while (inByte != '\r');
    Serial.println();
    Serial.print("BUFFER OVERFLOW");
    sendERROR();
    inDataLen = 0;
  } else {
    inByte = toupper(inByte);
    if (inByte < ' ') {
      do {
        inByte = Serial.read();
      } while (inByte != '\r');
      Serial.println();
      Serial.print("BAD BYTE RECEIVED:");
      Serial.print(inByte);
      sendERROR();
      inDataLen = 0;
    }
    inBuffer[inDataLen++] = inByte;
  }
}

void process_cmd() {
  Serial.println();

  if (!inDataLen) {
    sendOK();
    return;
  }

  byte csum = checkChecksum();
  if (csum) {
    Serial.print("BAD CHECKSUM:");
    printHexln(0 - csum);
    sendERROR();
    return;
  }

  // Remove checksum from length
  inDataLen -= 2;

  switch (inBuffer[0]) {
    case 'D': CMD_ENTER_DEBUG();    break;
    case 'L': CMD_LED();            break;
    case 'R': CMD_RESET();          break;
    case 'M': CMD_XDATA();          break;
    case 'X': CMD_EXTENDED();       break;
    default:
      Serial.print("BAD COMMAND:");
      Serial.println(inBuffer[0]);
      sendERROR();
  }
}

void CMD_ENTER_DEBUG() {
  if (inDataLen != 1) {
    sendERROR();
    return;
  }

  dbg_enter();
  dbg_instr(0x00);
  sendOK();
}

void CMD_LED() {
  if (inDataLen != 2) {
    sendERROR();
    return;
  }

  switch (inBuffer[1]) {
    case '0': LED_OFF();                                    break;
    case '1': LED_ON();                                     break;
    case '3': digitalWrite(CC_DC, HIGH);    				break;
    case '4': digitalWrite(CC_DC, LOW);    					break;
    case '5': digitalWrite(CC_DD, HIGH);    				break;
    case '6': digitalWrite(CC_DD, LOW);    					break;
    case '7': digitalWrite(CC_RST, LOW);        			break;
    case '8': digitalWrite(CC_RST, LOW);     				break;
  }
  sendOK();
}

void CMD_RESET() {
  if (inDataLen != 2) {
    sendERROR();
    return;
  }

  switch (inBuffer[1]) {
    case '0': dbg_reset(0);   break;
    case '1': dbg_reset(1);   break;
  }
  sendOK();
}

void CMD_XDATA() {
  if (inDataLen < 8 || !isHexByte(2) || !isHexByte(4) || !isHexByte(6)) {
    sendERROR();
    return;
  }

  byte cnt = getHexByte(6);
  if (!cnt) {
    sendOK();
    return;
  }
  if (cnt > 120) {
    Serial.println("NO MORE THAN 120 BYTES");
    sendERROR();
    return;
  }

  LED_ON();
  switch (inBuffer[1]) {
    case 'W': CMD_XDATA_WRITE(cnt);                   break;
    case 'R': CMD_XDATA_READ(cnt, READ_TYPE_XDATA);   break;
    case 'C': CMD_XDATA_READ(cnt, READ_TYPE_CODE);    break;
    default:
      sendERROR();
  }
  LED_OFF();
}

void CMD_XDATA_WRITE(byte cnt) {
  idx = 8;
  dbg_instr(0x90, getHexByte(2), getHexByte(4));      // MOV DPTR #high #low
  while (cnt-- > 0) {
    if (idx + 1 >= inDataLen) {
      Serial.println("NO DATA");
      sendERROR();
      return;
    }
    if (!isHexByte(idx)) {
      Serial.println("NO HEX");
      sendERROR();
      return;
    }

    dbg_instr(0x74, getHexByte(idx));
    dbg_instr(0xF0);
    dbg_instr(0xA3);
    idx += 2;
  }
  sendOK();
}

void CMD_XDATA_READ(byte cnt, byte type) {
  byte data, csum = 0;
  dbg_instr(0x90, getHexByte(2), getHexByte(4));      // MOV DPTR #high #low
  Serial.print("READ:");
  while (cnt-- > 0) {
    if (type == READ_TYPE_XDATA)
      data = dbg_instr(0xE0);
    else {
      dbg_instr(0xE4);
      data = dbg_instr(0x93);
    }
    dbg_instr(0xA3);
    csum += data;
    printHex(data);
  }
  csum = (0 - (~csum));
  printHexln(csum);
  sendOK();
}

void CMD_EXTENDED() {
  LED_ON();
  idx = 1;
  bool ok = true;
  while (ok && idx < inDataLen) {
    switch (inBuffer[idx++]) {
      case 'W': ok = CMD_EXTENDED_WRITE();  break;
      case 'R': ok = CMD_EXTENDED_READ();   break;
      default:
        ok = false;
    }
  }
  LED_OFF();
  if (ok) {
    sendOK();
  } else {
    sendERROR();
  }
}

bool CMD_EXTENDED_WRITE() {
  if (idx >= inDataLen || !isdigit(inBuffer[idx]))
    return false;

  byte cnt = inBuffer[idx++] - '0';
  if (!cnt)
    return true;
  while (cnt-- > 0) {
    if (idx + 1 >= inDataLen || !isHexByte(idx))
      return false;
    dbg_write(getHexByte(idx));
    idx += 2;
  }
  return true;
}

bool CMD_EXTENDED_READ() {
  if (idx >= inDataLen || !isdigit(inBuffer[idx]))
    return false;

  Serial.print("READ:");
  byte cnt = inBuffer[idx++] - '0';
  byte csum = 0;
  dbg_begin_response();
  while (cnt-- > 0) {
    byte data = dbg_read();
    csum += data;
    printHex(data);
  }
  dbg_end_response();
  csum = (0 - (~csum));
  printHexln(csum);
  return true;
}

byte isHexDigit(unsigned char c) {
  if (c >= '0' && c <= '9')
    return 1;
  else if (c >= 'A' && c <= 'F')
    return 1;
  return 0;
}

byte isHexByte(byte index) {
  return isHexDigit(inBuffer[index]) & isHexDigit(inBuffer[index + 1]);
}

byte getHexDigit(unsigned char c) {
  if (c >= '0' && c <= '9')
    c -= '0';
  else if (c >= 'A' && c <= 'F')
    c -= ('A' - 10);
  return c;
}

byte getHexByte(byte index) {
  return ((getHexDigit(inBuffer[index]) << 4) | getHexDigit(inBuffer[index + 1]));
}

byte checkChecksum() {
  if (inDataLen <= 2)
    return 0;

  byte csum = 0;
  byte imax = inDataLen - 2;
  byte i = 0;
  for (; i < imax; ++i)
    csum += inBuffer[i];
  csum = ~csum;
  return (csum + getHexByte(i));
}

void LED_OFF() {
  digitalWrite(LED, 0);
}

void LED_ON() {
  digitalWrite(LED, 1);
}

void LED_TOGGLE() {
  digitalWrite(LED, !digitalRead(LED));
}

void BlinkLED(byte blinks) {
  while (blinks-- > 0) {
    LED_ON();
    delay(500);
    LED_OFF();
    delay(500);
  }
  delay(1000);
}

void cc_delay( unsigned char d ) {
  volatile unsigned char i = d;
  while ( i-- );
}

inline void dbg_clock_high() {
  digitalWrite(CC_DC, 1);
}

inline void dbg_clock_low() {
  digitalWrite(CC_DC, 0);
}

// 1 - activate RESET (low)
// 0 - deactivate RESET (high)
void dbg_reset(unsigned char state) {
  if (state) {
    digitalWrite(CC_RST, 0);
  } else {
    digitalWrite(CC_RST, 1);
  }
  cc_delay(200);
}

void dbg_enter() {
  dbg_reset(1);
  dbg_clock_high();
  cc_delay(1);
  dbg_clock_low();
  cc_delay(1);
  dbg_clock_high();
  cc_delay(1);
  dbg_clock_low();
  cc_delay(200);
  dbg_reset(0);
}

void dbg_begin_response() {
  pinMode(CC_DD, INPUT);
  digitalWrite(CC_DD, LOW);
  asm("nop");
  asm("nop"); // Need to delay for at least 83 ns.
  // Wait for the interface to be ready.
  // This is primarily needed by the 'Gen2' CC25xx devices which have the ability to signal a delayed response
  //  by setting the data line high when the programmer switches from write -> read mode.
  while (digitalRead(CC_DD)) {
    for (byte cnt = 8; cnt; cnt--) {
      digitalWrite(CC_DC, HIGH);
      digitalWrite(CC_DC, LOW);
    }
  }
}

void dbg_end_response() {
  pinMode(CC_DD, OUTPUT);
  digitalWrite(CC_DD, LOW);
}

byte dbg_read() {
  byte data;
  for (byte cnt = 8; cnt; cnt--) {
    dbg_clock_high();
    data <<= 1;
    asm("nop");
    asm("nop");
    asm("nop");
    if (digitalRead(CC_DD))
      data |= 0x01;
    dbg_clock_low();
  }
  return data;
}

void dbg_write(byte data) {
  for (byte cnt = 8; cnt; cnt--) {
    if (data & 0x80)
      digitalWrite(CC_DD, HIGH);
    else
      digitalWrite(CC_DD, LOW);
    dbg_clock_high();
    data <<= 1;
    dbg_clock_low();
  }
}

char nibbleToHex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

void printHex(unsigned char data) {
  byte nibble1 = data >> 4;
  Serial.write(nibbleToHex[nibble1]);
  byte nibble2 = data & 0xF;
  Serial.write(nibbleToHex[nibble2]);
}

void printHexln(unsigned char data) {
  printHex(data);
  Serial.println("");
}

byte dbg_instr(byte in0, byte in1, byte in2) {
  dbg_write(0x57);
  dbg_write(in0);
  dbg_write(in1);
  dbg_write(in2);

  dbg_begin_response();
  byte response = dbg_read();
  dbg_end_response();
  return response;
}

byte dbg_instr(byte in0, byte in1) {
  dbg_write(0x56);
  dbg_write(in0);
  dbg_write(in1);

  dbg_begin_response();
  byte response = dbg_read();
  dbg_end_response();
  return response;
}

byte dbg_instr(byte in0) {
  dbg_write(0x55);
  dbg_write(in0);

  dbg_begin_response();
  byte response = dbg_read();
  dbg_end_response();
  return response;
}

void sendERROR() {
  Serial.print("ERROR\r\n:");
}

void sendOK() {
  Serial.print("OK\r\n:");
}
