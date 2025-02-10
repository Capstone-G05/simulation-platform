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

#define I2C_DEVICE "/dev/i2c-1"  // I2C bus 1 on Raspberry Pi
#define DAC_I2C_ADDRESS 0x60     // DAC I2C address
#define TX_BUFFER_SIZE 3         // Transmit buffer size for DAC communication
#define RX_BUFFER_SIZE 2         // Receive buffer size for DAC communication
#define MESSAGE_DELAY_US 25000   // 25ms delay for slave response

const uint8_t DAC_WRITE_CMD_MASK = 0x00;  // "00" for writing
const uint8_t DAC_READ_CMD_MASK = 0x06;   // "11" for reading

int i2c_fd = -1;  // Global file descriptor for I2C

void cleanup_and_exit(int exit_code) {
  if (i2c_fd >= 0) {
    close(i2c_fd);
    printf("I2C file closed.\n");
  }
  exit(exit_code);
}

void handle_sigint(int sig) {
  printf("\nCaught signal %d. Cleaning up and exiting.\n", sig);
  cleanup_and_exit(EXIT_SUCCESS);
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

int write_dac(uint8_t register_index, uint16_t value) {
  if (register_index > 0x1F) {
    printf("Invalid register index. Must be between 0 and 31.\n");
    return -1;
  }

  unsigned char tx_buffer[TX_BUFFER_SIZE];

  // Build the DAC write command
  tx_buffer[0] = ((register_index & 0x1F) << 3) | DAC_WRITE_CMD_MASK;  // Register number with write command bits
  tx_buffer[1] = (value >> 8) & 0xFF;  // High byte of data
  tx_buffer[2] = value & 0xFF;         // Low byte of data

  if (write(i2c_fd, tx_buffer, TX_BUFFER_SIZE) != TX_BUFFER_SIZE) {
    perror("Failed to write bytes to the DAC");
    return -1;
  }

  printf("Sent write command to DAC: %02X %02X %02X\n", tx_buffer[0], tx_buffer[1], tx_buffer[2]);
  return 0;
}

int read_dac(uint8_t register_index, uint16_t *value) {
  if (register_index > 0x1F) {
    printf("Invalid register index. Must be between 0 and 31.\n");
    return -1;
  }

  unsigned char tx_buffer[1];
  unsigned char rx_buffer[RX_BUFFER_SIZE];

  // Build the DAC read command
  tx_buffer[0] = ((register_index & 0x1F) << 3) | DAC_READ_CMD_MASK;  // Register number with read command bits

  if (write(i2c_fd, tx_buffer, 1) != 1) {
    perror("Failed to send read request to DAC");
    return -1;
  }

  usleep(MESSAGE_DELAY_US);  // Wait for the response

  if (read(i2c_fd, rx_buffer, RX_BUFFER_SIZE) != RX_BUFFER_SIZE) {
    perror("Failed to read bytes from the DAC");
    return -1;
  }

  *value = (rx_buffer[0] << 8) | rx_buffer[1];

  printf("Received read response from DAC: %02X %02X\n", rx_buffer[0], rx_buffer[1]);
  return 0;
}

void print_help() {
  printf("\nAvailable commands:\n");
  printf(" W <register> <value> - Write value to a specific register (0 to 31)\n");
  printf(" R <register> - Read current value from a specific register (0 to 31)\n");
  printf(" H - Show this help menu\n");
  printf(" Q - Quit the program\n");
}

int main() {
  char user_input[50];
  uint8_t register_index;
  uint16_t data_value;

  signal(SIGINT, handle_sigint);  // Register signal handler for Ctrl+C

  // Open the I2C device
  if ((i2c_fd = open_i2c_device()) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  // Set the I2C slave address
  if (set_i2c_slave_address(i2c_fd, DAC_I2C_ADDRESS) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  printf("DAC I2C Communication Program. Type 'h' for help.\n");

  while (1) {
    printf("\nEnter a command: ");
    if (fgets(user_input, sizeof(user_input), stdin) == NULL) {
      printf("Failed to read input.\n");
      continue;
    }

    user_input[strcspn(user_input, "\n")] = '\0';

    if (strcmp(user_input, "Q") == 0 || strcmp(user_input, "q") == 0) {
      printf("Exiting...\n");
      cleanup_and_exit(EXIT_SUCCESS);
    }

    if (strcmp(user_input, "H") == 0 || strcmp(user_input, "h") == 0) {
      print_help();
      continue;
    }

    if (user_input[0] == 'W' || user_input[0] == 'w') {
      if (sscanf(user_input + 1, "%hhu %hu", &register_index, &data_value) != 2) {
        printf("Invalid input format. Provide a register (0 to 31) and a 16-bit integer value after 'W'.\n");
        continue;
      }

      if (write_dac(register_index, data_value) < 0) {
        cleanup_and_exit(EXIT_FAILURE);
      }

    } else if (user_input[0] == 'R' || user_input[0] == 'r') {
      if (sscanf(user_input + 1, "%hhu", &register_index) != 1) {
        printf("Invalid input format. Provide a register (0 to 31) after 'R'.\n");
        continue;
      }

      if (read_dac(register_index, &data_value) < 0) {
        cleanup_and_exit(EXIT_FAILURE);
      }
      printf("Current DAC value at register %u: %u\n", register_index, data_value);

    } else {
      printf("Invalid command. Type 'h' for help.\n");
    }
  }

  cleanup_and_exit(EXIT_SUCCESS);
}
