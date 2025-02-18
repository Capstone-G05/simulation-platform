#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define I2C_DEVICE "/dev/i2c-1"
#define STM32_ADDRESS 0x10
#define TX_BUFFER_SIZE 4
#define RX_BUFFER_SIZE 4
#define REFRESH_RATE_US 1000000

typedef struct {
  uint8_t read_write;
  uint8_t type;
  uint8_t index;
  char description[50];
  char units[10];
} MessageMapping;

const MessageMapping message_table[] = {
  { 1, 0x02, 0x08, "Auger Pivot Up",      "%"   },
  { 1, 0x02, 0x04, "Auger Pivot Down",    "%"   },
  { 1, 0x02, 0x0B, "Auger Unfold",        "%"   },
  { 1, 0x02, 0x0A, "Auger Fold",          "%"   },
  { 1, 0x02, 0x01, "Spout Tilt Up",       "%"   },
  { 1, 0x02, 0x03, "Spout Tilt Down",     "%"   },
  { 1, 0x02, 0x06, "Spout Rotate CW",     "%"   },
  { 1, 0x02, 0x07, "Spout Rotate CCW",    "%"   },
  { 1, 0x02, 0x05, "Gate Open",           "%"   },
  { 1, 0x02, 0x09, "Gate Close",          "%"   },
  { 1, 0x02, 0x00, "Tandem Float",        "I/O" },
  { 1, 0x02, 0x02, "Tandem Cutoff",       "I/O" },
  { 1, 0x00, 0x01, "Read User LED state", "NA"  },
  { 1, 0x04, 0x00, "STM32 Status",        "NA"  },
};

const size_t MESSAGE_TABLE_SIZE = sizeof(message_table) / sizeof(MessageMapping);

int i2c_fd = -1;

void cleanup_and_exit(int exit_code) {
  if (i2c_fd >= 0) {
    close(i2c_fd);
    printf("I2C file closed.\n");
  }
  exit(exit_code);
}

int open_i2c_device() {
  int fd = open(I2C_DEVICE, O_RDWR);
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

int read_bytes(int fd, unsigned char *buffer, size_t size) {
  if (read(fd, buffer, size) != size) {
    perror("Failed to read bytes from the I2C bus");
    return -1;
  }
  return 0;
}

void print_table_header() {
  printf("\033[H"); // Move cursor to home position
  printf("%-25s %-10s %-10s\n", "Description", "Value", "Units");
  printf("------------------------------------------------------\n");
}

void update_table() {
  unsigned char tx_buffer[TX_BUFFER_SIZE] = {0};
  unsigned char rx_buffer[RX_BUFFER_SIZE] = {0};

  print_table_header();

  for (size_t i = 0; i < MESSAGE_TABLE_SIZE; i++) {
    tx_buffer[0] = (message_table[i].read_write << 7) | (message_table[i].type & 0x7F);
    tx_buffer[1] = message_table[i].index;
    tx_buffer[2] = 0;
    tx_buffer[3] = 0;

    if (write(i2c_fd, tx_buffer, TX_BUFFER_SIZE) != TX_BUFFER_SIZE) {
      printf("%-25s ERROR          %s\n", message_table[i].description, message_table[i].units);
      continue;
    }

    if (read_bytes(i2c_fd, rx_buffer, RX_BUFFER_SIZE) < 0) {
      printf("%-25s ERROR          %s\n", message_table[i].description, message_table[i].units);
      continue;
    }

    uint16_t value = (rx_buffer[2] << 8) | rx_buffer[3];
    printf("%-25s %-10d %-10s\n", message_table[i].description, value, message_table[i].units);

    usleep(REFRESH_RATE_US/MESSAGE_TABLE_SIZE);
  }
  fflush(stdout); // Ensure output is updated immediately
}

void handle_sigint(int sig) {
  printf("\nCaught signal %d. Cleaning up and exiting.\n", sig);
  cleanup_and_exit(EXIT_SUCCESS);
}

int main() {
  signal(SIGINT, handle_sigint);

  if ((i2c_fd = open_i2c_device()) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  if (set_i2c_slave_address(i2c_fd, STM32_ADDRESS) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  printf("\033[J");
  while (1) {
    update_table();
  }

  cleanup_and_exit(EXIT_SUCCESS);
}
