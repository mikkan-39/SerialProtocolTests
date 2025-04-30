#include <stdio.h>

#include "hardware/uart.h"
#include "pico/stdlib.h"
#include "stdio.h"

#define UART_ID uart0
#define BAUD_RATE 1000000

#define START_BYTE 0xAA

// CRC8
uint8_t calculate_checksum(uint8_t* data, int length) {
  char crc = 0x00;
  char extract;
  char sum;
  for (int i = 0; i < length; i++) {
    extract = *data;
    for (char tempI = 8; tempI; tempI--) {
      sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) crc ^= 0x8C;
      extract >>= 1;
    }
    data++;
  }
  return crc;
}

bool validate_checksum(uint8_t* data, int length) {
  uint8_t crc = calculate_checksum(data, length);
  return crc == data[length - 1];
}

void send_ack() {
  uint8_t packet[] = {START_BYTE, 0x01, 0x00, 0x01};
  packet[3] = calculate_checksum(packet, 4);
  uart_write_blocking(uart0, packet, sizeof(packet));
}

void send_status() {
  uint8_t status = 0;  // get_status();
  uint8_t packet[] = {START_BYTE, 0x02, 0x01, status, 0x00};
  packet[4] = calculate_checksum(packet, 5);
  uart_write_blocking(uart0, packet, sizeof(packet));
}

void send_sensor_data() {}
void send_error() {}
void set_parameters(int32_t a, int32_t b) {}

void handle_packet(uint8_t type, uint8_t length, const uint8_t* payload) {
  switch (type) {
    case 0x01:  // ACK
      send_ack();
      break;
    case 0x02:  // Get Status
      send_status();
      break;
    case 0x03:  // Get Sensor Data
      send_sensor_data();
      break;
    case 0x04:  // Set Parameters
      if (length == 8) {
        int32_t p1 = *(int32_t*)&payload[0];
        int32_t p2 = *(int32_t*)&payload[4];
        set_parameters(p1, p2);
        send_ack();
      } else {
        send_error();
      }
      break;
    default:
      send_error();
      break;
  }
}

void on_serial_rx(uint8_t byte) {
  static uint8_t buffer[256];
  static uint8_t pos = 0;
  static uint8_t expected_len = 0;
  static bool syncing = true;

  if (syncing) {
    if (byte == START_BYTE) {
      pos = 0;
      buffer[pos++] = byte;
      syncing = false;
    }
    return;
  }

  buffer[pos++] = byte;

  if (pos == 2) {
    // Received PACKET_TYPE
  } else if (pos == 3) {
    // Received PACKET_LENGTH
    expected_len = buffer[2];
  } else if (pos == 3 + expected_len + 1) {
    // Full packet + checksum received
    if (validate_checksum(buffer, pos)) {
      handle_packet(buffer[1], buffer[2], buffer + 3);
    }
    syncing = true;
  }

  if (pos >= sizeof(buffer)) {
    syncing = true;  // Avoid overflow
  }
}

int main() {
  stdio_init_all();
  uart_init(UART_ID, BAUD_RATE);

  while (true) {
    if (uart_is_readable(UART_ID)) {
      uart_getc(UART_ID);
    }
  }
}
