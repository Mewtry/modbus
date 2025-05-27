#include <Arduino.h>
#line 1 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
#define SLAVE_ID 1
#define TX_ENABLE_PIN 2
#define BAUD_RATE 9600


#define NUMCOILS 6
uint8_t coils[NUMCOILS] = {13, 12, 11, 10, 9, 8};

#define NUM_REGISTERS 10
uint16_t holdingRegisters[NUM_REGISTERS] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};

#line 12 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
void setup();
#line 25 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
void loop();
#line 84 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
bool handleReadCoils(uint8_t* request, uint8_t* response, uint8_t* responseLength);
#line 112 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
bool handleModbusFunctionException(uint8_t functionCode, uint8_t* response, uint8_t* responseLength);
#line 125 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
bool handleReadHoldingRegisters(uint8_t* request, uint8_t* response, uint8_t* responseLength);
#line 148 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
bool handleWriteSingleCoil(uint8_t* request, uint8_t* response, uint8_t* responseLength);
#line 185 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
bool handleWriteSingleRegister(uint8_t* request, uint8_t* response, uint8_t* responseLength);
#line 203 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
uint16_t calculateCRC(uint8_t *data, uint8_t length);
#line 218 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
bool verifyCRC(uint8_t* data, uint8_t len, uint8_t crcMSB, uint8_t crcLSB);
#line 229 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
void sendModbusResponse(uint8_t* data, uint8_t len);
#line 12 "C:\\workspace\\IFSC\\RIS\\modbus\\modbus\\modbus.ino"
void setup() {
  pinMode(TX_ENABLE_PIN, OUTPUT);
  digitalWrite(TX_ENABLE_PIN, LOW);
  // for (int i = 0; i < NUMCOILS; i++) {
  //   pinMode(coils[i], OUTPUT);
  //   digitalWrite(coils[i], LOW); // Inicializa bobinas como desligadas
  // }
  DDRB = 0b00111111; // Pinos de D8 a D13 como saída (PB0 a PB5)
  PORTB = 0b00100000; // LED D13 Ligado
  Serial.begin(BAUD_RATE);
  // Serial.println("Initialized");
}

void loop() {
  if (Serial.available() >= 8) {
    uint8_t request[256];
    uint8_t index = 0;

    while (Serial.available() && index < 256) {
      request[index++] = Serial.read();
      delayMicroseconds(100); // Aguarda bytes seguintes
    }

    // Serial.print("DEVIDE ID: ");
    // Serial.println(request[0], HEX);
    // Serial.print("Function Code: ");
    // Serial.println(request[1], HEX);
    // Serial.print("ADDR: ");
    // Serial.print(request[2], HEX);
    // Serial.print(" ");
    // Serial.println(request[3], HEX);
    // Serial.print("NUM: ");
    // Serial.print(request[4], HEX);
    // Serial.print(" ");
    // Serial.println(request[5], HEX);
    // Serial.print("CRC: ");
    // Serial.print(request[index - 2], HEX);
    // Serial.print(" ");
    // Serial.println(request[index - 1], HEX);
    // Serial.print("Index: ");
    // Serial.println(index);

    if (index < 8 || request[0] != SLAVE_ID) return;
    // Serial.println("Slave ID OK");
    if (!verifyCRC(request, index - 2, request[index - 2], request[index - 1])) return;
    // Serial.println("CRC OK");
    uint8_t response[256];
    uint8_t responseLength = 0;

    switch (request[1]) {
      case 0x01:
        if (!handleReadCoils(request, response, &responseLength)) return;
        break;
      case 0x03:
        if (!handleReadHoldingRegisters(request, response, &responseLength)) return;
        break;
      case 0x05:
        if (!handleWriteSingleCoil(request, response, &responseLength)) return;
        break;
      case 0x06:
        if (!handleWriteSingleRegister(request, response, &responseLength)) return;
        break;
      default: // Função não suportada
        if (!handleModbusFunctionException(request[1], response, &responseLength)) return;
        break; 
    }

    sendModbusResponse(response, responseLength);
  }
}

// Função 0x01 - Read Coils
bool handleReadCoils(uint8_t* request, uint8_t* response, uint8_t* responseLength) {
  uint16_t startAddr = (request[2] << 8) | request[3];
  uint16_t numCoils  = (request[4] << 8) | request[5];
  if(0x0001 > numCoils || numCoils > 0x07D0) return false; // Número de bobinas inválido | CodeError 0x03
  if(startAddr + numCoils > NUM_REGISTERS) return false;   // Leitura de coils inválida  | CodeError 0x02

  response[0] = SLAVE_ID;
  response[1] = 0x01; // Função de leitura de bobinas
  response[2] = numCoils / 8 + (numCoils % 8 ? 1 : 0); // Número de bytes a serem enviados

  uint8_t val = 0;
  PORTB = 0x00;
  for(int i = 0; i < numCoils; i++) {
    val = digitalRead(coils[(startAddr - 1) + i]);
    Serial.print("Pino ");
    Serial.print(coils[(startAddr - 1) + i]);
    Serial.print(": ");
    Serial.println(val);
    response [3 + i / 8] |= (val << (i % 8));
  }

  uint16_t crc = calculateCRC(response, 3 + response[2]);
  response[3 + response[2]] = crc & 0xFF;
  response[4 + response[2]] = crc >> 8;
  *responseLength = 5 + response[2];
  return true;
}

bool handleModbusFunctionException(uint8_t functionCode, uint8_t* response, uint8_t* responseLength) {
  response[0] = SLAVE_ID;
  response[1] = functionCode | 0x80;
  response[2] = 0x01;
  
  uint16_t crc = calculateCRC(response, 3);
  response[3] = crc & 0xFF;
  response[4] = crc >> 8;
  *responseLength = 5;
  return true;
}

// Função 0x03 - Read Holding Registers
bool handleReadHoldingRegisters(uint8_t* request, uint8_t* response, uint8_t* responseLength) {
  uint16_t startAddr = (request[2] << 8) | request[3];
  uint16_t numRegs   = (request[4] << 8) | request[5];
  if (startAddr + numRegs > NUM_REGISTERS) return false;

  response[0] = SLAVE_ID;
  response[1] = 0x03;
  response[2] = numRegs * 2;

  for (int i = 0; i < numRegs; i++) {
    uint16_t val = holdingRegisters[startAddr + i];
    response[3 + 2 * i] = val >> 8;
    response[4 + 2 * i] = val & 0xFF;
  }

  uint16_t crc = calculateCRC(response, 3 + 2 * numRegs);
  response[3 + 2 * numRegs] = crc & 0xFF;
  response[4 + 2 * numRegs] = crc >> 8;
  *responseLength = 5 + 2 * numRegs;
  return true;
}

// Função 0x05 - Write Single Coil
bool handleWriteSingleCoil(uint8_t* request, uint8_t* response, uint8_t* responseLength) {
  uint16_t coilAddr = (request[2] << 8) | request[3]; 
  uint16_t value    = (request[4] << 8) | request[5];
  // Serial.print("Coil Addr: ");
  // Serial.println(coilAddr, HEX);
  // Serial.print("Value: ");
  // Serial.println(value, HEX);
  if (coilAddr >= NUMCOILS) return false;
  if (value == 0x0000) {
    value = 0; // Desliga a bobina
  } else if (value == 0xFF00) {
    value = 1; // Liga a bobina
  } else {
    return false; // Valor inválido
  }
  // Serial.print("Value: ");
  // Serial.println(value);
  // Atualiza o estado da bobina
  digitalWrite(coils[coilAddr], value);
  if(digitalRead(coils[coilAddr]) != value) {
    return false; // Falha ao atualizar o estado da bobina
  }

  // Eco da requisição como resposta
  for (int i = 0; i < 6; i++) {
    response[i] = request[i];
  }

  uint16_t crc = calculateCRC(response, 6);
  response[6] = crc & 0xFF;
  response[7] = crc >> 8;
  *responseLength = 8;
  return true;

}

// Função 0x06 - Write Single Register
bool handleWriteSingleRegister(uint8_t* request, uint8_t* response, uint8_t* responseLength) {
  uint16_t regAddr = (request[2] << 8) | request[3]; 
  uint16_t value   = (request[4] << 8) | request[5];
  if (regAddr >= NUM_REGISTERS) return false;  

  // Eco da requisição como resposta
  for (int i = 0; i < 6; i++) {
    response[i] = request[i];
  }

  uint16_t crc = calculateCRC(response, 6);
  response[6] = crc & 0xFF;
  response[7] = crc >> 8;
  *responseLength = 8;
  return true;
}

// Cálculo CRC-16 Modbus
uint16_t calculateCRC(uint8_t *data, uint8_t length) {
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < length; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 0x0001)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc = crc >> 1;
    }
  }
  return crc;
}

// Verificação de CRC
bool verifyCRC(uint8_t* data, uint8_t len, uint8_t crcMSB, uint8_t crcLSB) {
  uint16_t calc = calculateCRC(data, len);
  uint16_t received = (crcMSB << 8) | crcLSB;
  // Serial.print("CRC Calculado: ");
  // Serial.print(calc, HEX);
  // Serial.print(" CRC Recebido: ");
  // Serial.println(received, HEX);
  return calc == received;
}

// Envio da resposta
void sendModbusResponse(uint8_t* data, uint8_t len) {
  digitalWrite(TX_ENABLE_PIN, HIGH);
  delayMicroseconds(10);
  Serial.write(data, len);
  Serial.flush();
  delayMicroseconds(10);
  digitalWrite(TX_ENABLE_PIN, LOW);
}

