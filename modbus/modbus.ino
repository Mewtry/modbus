#include <Arduino.h>

#define SLAVE_ID 1
#define TX_ENABLE_PIN 2
#define BAUD_RATE 9600


#define NUM_COILS 6
uint8_t coils[NUM_COILS] = {8, 9, 10, 11, 12, 13}; // Pinos D8 a D13 | PB0 a PB5

#define NUM_DISCRETE_INPUTS 6
uint8_t inputs[NUM_DISCRETE_INPUTS] = {2, 3, 4, 5, 6, 7}; // Pinos D2 a D7 | PD2 a PD7

#define NUM_INPUT_REGISTERS 6
uint16_t inputRegisters[NUM_INPUT_REGISTERS] = {14, 15, 16, 17, 18, 19}; // Pinos D14 a D19 | PC0 a PC5

#define NUM_REGISTERS 10
uint16_t holdingRegisters[NUM_REGISTERS] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};

void setup() {
  pinMode(TX_ENABLE_PIN, OUTPUT);
  digitalWrite(TX_ENABLE_PIN, LOW);
  DDRB |= 0b00111111; // Pinos de D8 a D13 como saída (PB0 a PB5)
  DDRC = 0b00000000;  // Pinos de D14 a D19 como entrada (PC0 a PC5)
  DDRD = 0b00000000;  // Pinos de D2 a D7 como entrada (PD2 a PD7)
  // PORTB |= 0b00100000; // LED D13 Ligado
  Serial.begin(BAUD_RATE);
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
    uint8_t response[256] = {0};
    uint8_t responseLength = 0;

    switch (request[1]) {
      case 0x01:
        if (!handleReadCoils(request, response, &responseLength)) return;
        break;
      case 0x02:
        if (!handleReadDiscreteInputs(request, response, &responseLength)) return;
        break;
      case 0x03:
        if (!handleReadHoldingRegisters(request, response, &responseLength)) return;
        break;
      case 0x04:
        if (!handleReadInputRegisters(request, response, &responseLength)) return;
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
// Exemplo de requisição: 01 01 00 01 00 06 C8 ED | Lê todas as 6 bobinas 
// Exemplo de requisição: 01 01 00 03 00 04 C9 CD | Lê as 4 últimas bobinas 
// Exemplo de requisição: 01 01 00 01 00 03 CB 2D | Lê as 3 primeiras bobinas 
bool handleReadCoils(uint8_t* request, uint8_t* response, uint8_t* responseLength) { 
  uint16_t startAddr = (request[2] << 8) | request[3];
  uint16_t numCoils  = (request[4] << 8) | request[5];
  if(0x0001 > numCoils || numCoils > 0x07D0) return false; // Número de bobinas inválido | CodeError 0x03
  if((startAddr + numCoils - 1) > NUM_COILS) return false;   // Leitura de coils inválida  | CodeError 0x02

  response[0] = SLAVE_ID;
  response[1] = 0x01; // Função de leitura de bobinas
  response[2] = 1;    // Número de bytes a serem enviados
  response[3] = 0x00; // Inicializa o primeiro byte de resposta

  uint8_t val = 0;
  for(int i = 0; i < numCoils; i++) {
    val = digitalRead(coils[(startAddr - 1) + i]);
    response [3] |= (val << i);
  }
  
  uint16_t crc = calculateCRC(response, 3 + response[2]);
  response[3 + response[2]] = crc & 0xFF;
  response[4 + response[2]] = crc >> 8;
  *responseLength = 5 + response[2];
  return true;
}

// Função 0x02 - Read Discrete Inputs
// Exemplo de requisição: 01 02 00 00 00 06 08 F8 | Lê todos os 6 inputs digitais
bool handleReadDiscreteInputs(uint8_t* request, uint8_t* response, uint8_t* responseLength) {
  uint16_t startAddr = (request[2] << 8) | request[3];
  uint16_t numInputs  = (request[4] << 8) | request[5];
  if(0x0000 > numInputs || numInputs > 0x07D0) return false; // Número de inputs inválido 
  if((startAddr + numInputs) > NUM_DISCRETE_INPUTS) return false;   // Leitura de inputs inválida

  response[0] = SLAVE_ID;
  response[1] = 0x02; // Função de leitura de inputs digitais
  response[2] = 0x01; // Número de bytes a serem enviados
  response[3] = 0x00; // Inicializa o primeiro byte de resposta
  
  uint8_t val = 0;
  for(int i = 0; i < numInputs; i++) {
    val = digitalRead(inputs[(startAddr) + i]);
    response [3] |= (val << i);
  }

  uint16_t crc = calculateCRC(response, 3 + response[2]);
  response[3 + response[2]] = crc & 0xFF;
  response[4 + response[2]] = crc >> 8;
  *responseLength = 5 + response[2];
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

// Função 0x04 - Read Input Registers
// Exemplo de requisição: 01 04 00 00 00 01 CA 31 | Lê o registrador de entrada A0
// Exemplo de requisição: 01 04 00 00 00 06 08 70 | Lê os registradores de entrada A0 a A5
bool handleReadInputRegisters(uint8_t* request, uint8_t* response, uint8_t* responseLength) {
  uint16_t startAddr = (request[2] << 8) | request[3];
  uint16_t numInputs  = (request[4] << 8) | request[5];
  if(0x0000 > numInputs || numInputs > 0x07D0) return false; // Número de inputs inválido
  if((startAddr + numInputs) > NUM_INPUT_REGISTERS) return false;   // Leitura de inputs inválida

  response[0] = SLAVE_ID;
  response[1] = 0x04; // Função de leitura de registradores de entrada
  response[2] = numInputs * 2; // Número de bytes a serem enviados

  for (int i = 0; i < numInputs; i++) {
    uint16_t val = analogRead(inputRegisters[startAddr + i]);
    response[3 + 2 * i] = val >> 8;
    response[4 + 2 * i] = val & 0xFF;
  }

  uint16_t crc = calculateCRC(response, 3 + response[2]);
  response[3 + response[2]] = crc & 0xFF;
  response[4 + response[2]] = crc >> 8;
  *responseLength = 5 + response[2];
  return true;
}

// Função 0x05 - Write Single Coil
// Exemplo de requisição: 01 05 00 00 FF 00 3A 8C | Liga o PB0 (D8)
// Exemplo de requisição: 01 05 00 00 00 00 CA CD | Desliga o PB0 (D8)
// Exemplo de requisição: 01 05 00 05 FF 00 3B 9C | Liga o PB5 (D13)
// Exemplo de requisição: 01 05 00 05 00 00 CB DD | Desliga o PB5 (D13)
bool handleWriteSingleCoil(uint8_t* request, uint8_t* response, uint8_t* responseLength) {
  uint16_t coilAddr = (request[2] << 8) | request[3]; 
  uint16_t value    = (request[4] << 8) | request[5];

  if (coilAddr >= NUM_COILS) return false;
  if (value == 0x0000) {
    value = 0; 
  } else if (value == 0xFF00) {
    value = 1;
  } else {
    return false; // Valor inválido
  }

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
  if (value < 0 || value > 0xFFFF) return false; // Valor inválido

  // Atualiza o valor do registrador
  holdingRegisters[regAddr] = value;

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
