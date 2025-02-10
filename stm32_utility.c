#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define I2C_DEVICE "/dev/i2c-1"  // Default I2C bus
#define STM32_ADDRESS 0x10       // STM32 I2C slave address
#define DAC_ADDRESS 0x60         // DAC I2C slave address
#define MESSAGE_DELAY_US 25000   // 25ms delay for slave response

int i2c_fd = -1;  // Global file descriptor for I2C

typedef enum {
  READ = 1,
  WRITE = 0,
} ReadWrite;

typedef enum {
  TYPE_GPIO = 0x00,
  TYPE_DAC = 0x01,
  TYPE_PWM = 0x02,
  TYPE_CAN = 0x03,
  TYPE_MISC = 0x04
} PeripheralType;

typedef struct {
  uint8_t read_write : 1;
  uint8_t type : 7;
  uint8_t index;
  uint16_t value;
} StmMessage;

typedef struct {
  uint8_t address : 5;
  uint8_t control : 2;
  uint8_t reserved : 1;
  uint16_t value;
} DacMessage;

void cleanup_and_exit(int exit_code) {
  if (i2c_fd >= 0) {
    close(i2c_fd);
    printf("I2C file closed.\n");
  }
  exit(exit_code);
}

int open_i2c_device(const char *device_path) {
  int fd = open(device_path, O_RDWR);
  if (fd < 0) {
    perror("Failed to open the I2C bus");
  }
  return fd;
}

int set_i2c_slave_address(int fd, int address) {
  if (ioctl(fd, I2C_SLAVE, address) < 0) {
    perror("Failed to set I2C slave address");
    return -1;
  }
  return 0;
}

int write_bytes(int fd, unsigned char *bytes, size_t size) {
  printf("TX:");
  for (size_t i = 0; i < size; i++) {
    printf(" %02X", bytes[i]);
  }
  printf("\n");
  if (write(fd, bytes, size) != size) {
    perror("Failed to write bytes to the I2C bus");
    return -1;
  }
  return 0;
}

int read_bytes(int fd, unsigned char *buffer, size_t size) {
  if (read(fd, buffer, size) != size) {
    perror("Failed to read bytes from the I2C bus");
    return -1;
  }
  printf("RX:");
  for (size_t i = 0; i < size; i++) {
    printf(" %02X", buffer[i]);
  }
  printf("\n");
  return 0;
}

uint16_t stm_send(uint8_t type, uint8_t index, uint8_t read_write, uint16_t value) {
  if (set_i2c_slave_address(i2c_fd, STM32_ADDRESS) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  uint8_t tx_buffer[4] = {
    (read_write & 0x01) | ((type & 0x7F) << 1),
    index,
    (uint8_t)(value >> 8),
    (uint8_t)(value & 0xFF)
  };

  if (write_bytes(i2c_fd, tx_buffer, 4) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  usleep(MESSAGE_DELAY_US);

  uint8_t rx_buffer[4];
  if (read_bytes(i2c_fd, rx_buffer, 4) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  return (rx_buffer[2] << 8) | rx_buffer[3];
}

uint16_t dac_send(uint8_t address, uint8_t read_write, uint16_t value) {
  if (set_i2c_slave_address(i2c_fd, DAC_ADDRESS) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  if (read_write == WRITE) {
    uint8_t tx_buffer[3] = {
      (address & 0x1F) | ((read_write & 0x03) << 5),
      (uint8_t)(value >> 8),
      (uint8_t)(value & 0xFF)
    };

    if (write_bytes(i2c_fd, tx_buffer, 3) < 0) {
      cleanup_and_exit(EXIT_FAILURE);
    }
  }

  uint8_t tx_buffer[1] = {
    (address & 0x1F) | ((read_write & 0x03) << 5),
  };

  if (write_bytes(i2c_fd, tx_buffer, 3) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  usleep(MESSAGE_DELAY_US);

  uint8_t rx_buffer[2];
  if (read_bytes(i2c_fd, rx_buffer, 2) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }
  return (rx_buffer[0] << 8) | rx_buffer[1];
}

void i2c_message(uint8_t type, uint8_t index, uint8_t read_write, uint16_t value) {
  if (type == TYPE_DAC) {
    dac_send(index, read_write, value);
  } else {
    uint16_t result = stm_send(type, index, read_write, value);
    printf("Received result: 0x%04X\n", result);
  }
}

int main(int argc, char *argv[]) {
  if (argc != 5) {
    printf("Usage: %s <R/W> <TYPE> <INDEX> <VALUE>\n", argv[0]);
    return EXIT_FAILURE;
  }

  uint8_t read_write = (uint8_t)atoi(argv[1]);
  uint8_t type = (uint8_t)atoi(argv[2]);
  uint8_t index = (uint8_t)atoi(argv[3]);
  uint16_t value = (uint16_t)atoi(argv[4]);

  if ((i2c_fd = open_i2c_device(I2C_DEVICE)) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  i2c_message(type, index, read_write, value);

  cleanup_and_exit(EXIT_SUCCESS);
}
