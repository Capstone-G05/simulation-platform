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
#define STM32_ADDRESS 0x10       // STM32 I2C slave address
#define TX_BUFFER_SIZE 4         // Transmit buffer size
#define RX_BUFFER_SIZE 4         // Receive buffer size
#define MESSAGE_DELAY_US 25000   // 25ms delay for slave response

#define MAX_STRING_LENGTH  50
#define MAX_COMMAND_LENGTH 5

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

typedef enum {
  INDEX_PB4 = 0x00,    // LED 1
  INDEX_PB5 = 0x01,    // LED 2
  INDEX_PB12 = 0x02,   // PCB 23
} GPIOIndex;

typedef enum {
  INDEX_PA8 = 0x00,    // PCB 32
  INDEX_PB15 = 0x01,   // PCB 31
} FREQIndex

typedef enum {
  INDEX_PA0 = 0x00,    // PCB 1
  INDEX_PA1 = 0x01,    // PCB 2
  INDEX_PA2 = 0x02,    // PCB 3
  INDEX_PA3 = 0x03,    // PCB 4
  INDEX_PA4 = 0x04,    // PCB 5
  INDEX_PA5 = 0x05,    // PCB 6
  INDEX_PA6 = 0x06,    // PCB 7
  INDEX_PA7 = 0x07,    // PCB 8
  INDEX_PB0 = 0x08,    // PCB 9
  INDEX_PB1 = 0x09,    // PCB 10
  INDEX_PB2 = 0x0A,    // PCB 11
  INDEX_PB10 = 0x0B,   // PCB 12
} PWMIndex;

typedef enum {
  INDEX_LDC1 = 0x00,   // Load Cell 1
  INDEX_LDC2 = 0x01,   // Load Cell 2
  INDEX_LDC3 = 0x02,   // Load Cell 3
  INDEX_LDC4 = 0x03,   // Load Cell 4
  INDEX_LDC5 = 0x04,   // Load Cell 5
  INDEX_LDC6 = 0x05,   // Load Cell 6
  INDEX_LDC7 = 0x06,   // Load Cell 7
  INDEX_LDC8 = 0x07,   // Load Cell 8
  INDEX_LDC9 = 0x08,   // Load Cell 9
  INDEX_LDC10 = 0x09,  // Load Cell 10
  INDEX_LDC11 = 0x0A,  // Load Cell 11
  INDEX_LDC12 = 0x0B,  // Load Cell 12
} CANIndex;

typedef enum {
  INDEX_STATUS = 0x00, // STM32 Status
} MISCIndex;

typedef struct {
  char command[MAX_COMMAND_LENGTH];
  uint8_t read_write : 1;
  uint8_t type;
  uint8_t index;
  char description[MAX_STRING_LENGTH];
} MessageMapping;

const MessageMapping message_table[] = {
  { "APU", READ,  TYPE_PWM,  INDEX_PB0,    "Auger Pivot Up"       },
  { "APD", READ,  TYPE_PWM,  INDEX_PA4,    "Auger Pivot Down"     },
  { "AFU", READ,  TYPE_PWM,  INDEX_PB10,   "Auger Unfold"         },
  { "AFF", READ,  TYPE_PWM,  INDEX_PB2,    "Auger Fold"           },
  { "STU", READ,  TYPE_PWM,  INDEX_PA1,    "Spout Tilt Up"        },
  { "STD", READ,  TYPE_PWM,  INDEX_PA3,    "Spout Tilt Down"      },
  { "SRC", READ,  TYPE_PWM,  INDEX_PA6,    "Spout Rotate CW"      },
  { "SRW", READ,  TYPE_PWM,  INDEX_PA7,    "Spout Rotate CCW"     },
  { "GTO", READ,  TYPE_PWM,  INDEX_PA5,    "Gate Open"            },
  { "GTC", READ,  TYPE_PWM,  INDEX_PB1,    "Gate Close"           },
  { "PTO", WRITE, TYPE_FREQ, INDEX_PB15,   "PTO Speed"            },
  { "WFL", WRITE, TYPE_CAN,  INDEX_LDC1,   "Weight Front Left"    },
  { "WFR", WRITE, TYPE_CAN,  INDEX_LDC2,   "Weight Front Right"   },
  { "WRL", WRITE, TYPE_CAN,  INDEX_LDC3,   "Weight Rear Left"     },
  { "WRR", WRITE, TYPE_CAN,  INDEX_LDC4,   "Weight Rear Right"    },
  { "WHH", WRITE, TYPE_CAN,  INDEX_LDC5,   "Weight Hitch"         },
  { "TDF", READ,  TYPE_PWM,  INDEX_PA0,    "Tandem Float"         },
  { "TDC", READ,  TYPE_PWM,  INDEX_PA2,    "Tandem Cutoff"        },
  { "WSD", WRITE, TYPE_FREQ, INDEX_PA8,    "Wheel Speed"          },
  { "WDR", WRITE, TYPE_GPIO, INDEX_PB12,   "Wheel Direction"      },
  { "LED", WRITE, TYPE_GPIO, INDEX_PB5,    "User Controlled LED"  },
  { "LDR", READ,  TYPE_GPIO, INDEX_PB5,    "Read User LED state"  },
  { "STS", READ,  TYPE_MISC, INDEX_STATUS, "STM32 Status"         },
};

const size_t MESSAGE_TABLE_SIZE = sizeof(message_table) / sizeof(MessageMapping);

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

int write_bytes(int fd, unsigned char *bytes, size_t size) {
  if (write(fd, bytes, size) != size) {
    perror("Failed to write bytes to the I2C bus");
    return -1;
  }
  printf("Sent %lu byte(s):", size);
  for (size_t i = 0; i < size; i++) {
    printf(" %02X", bytes[i]);
  }
  printf(" to address: 0x%02X\n", STM32_ADDRESS);
  return 0;
}

int read_bytes(int fd, unsigned char *buffer, size_t size) {
  if (read(fd, buffer, size) != size) {
    perror("Failed to read bytes from the I2C bus");
    return -1;
  }
  printf("Received %lu byte(s):", size);
  for (size_t i = 0; i < size; i++) {
    printf(" %02X", buffer[i]);
  }
  printf(" from address: 0x%02X\n", STM32_ADDRESS);
  return 0;
}

int build_message(const MessageMapping *message, unsigned char *buffer, uint16_t value) {
  buffer[0] = (message->read_write << 7) | (message->type & 0x7F);
  buffer[1] = message->index;
  buffer[2] = (value >> 8) & 0xFF;
  buffer[3] = value & 0xFF;
  return 0;
}

void print_help() {
  printf("\nAvailable commands (with optional data values):\n");
  for (size_t i = 0; i < MESSAGE_TABLE_SIZE; i++) {
    printf(" %s <value> - %s\n",
        message_table[i].command,
        message_table[i].description);
  }
  printf(" H - Show this help menu\n");
  printf(" Q - Quit the program\n");
}

int parse_command_input(char *input, char *command, uint16_t *value) {
  char *token = strtok(input, " ");
  if (!token) return -1;

  // Copy the command and ensure it is null-terminated
  strncpy(command, token, MAX_COMMAND_LENGTH);
  command[MAX_COMMAND_LENGTH - 1] = '\0';

  token = strtok(NULL, " ");
  if (token) {
    // Reset errno before calling strtol
    errno = 0;

    // Convert token to a long value
    char *endptr;
    long converted_data = strtol(token, &endptr, 10);

    // Check for conversion errors
    if (endptr == token) {
      // No digits were found
      printf("Error: Invalid number '%s'.\n", token);
      return -1;
    }

    if (errno == ERANGE || converted_data < 0 || converted_data > UINT16_MAX) {
      // Error in conversion or out of range for uint16_t
      printf("Error: Value '%s' is out of range or invalid.\n", token);
      return -1;
    }

    // Successfully converted, assign the value to *data
    *value = (uint16_t)converted_data;
  } else {
    // If no data value, set to 0
    *value = 0;
  }

  return 0;
}

int main() {
  unsigned char tx_buffer[TX_BUFFER_SIZE] = {0};
  unsigned char rx_buffer[RX_BUFFER_SIZE] = {0};
  char user_input[MAX_STRING_LENGTH];
  char parsed_command[MAX_COMMAND_LENGTH];
  uint16_t data_value;

  signal(SIGINT, handle_sigint);  // Register signal handler for Ctrl+C

  // Open the I2C device
  if ((i2c_fd = open_i2c_device()) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  // Set the I2C slave address
  if (set_i2c_slave_address(i2c_fd, STM32_ADDRESS) < 0) {
    cleanup_and_exit(EXIT_FAILURE);
  }

  printf("I2C Communication Program. Type 'h' for help.\n");

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

    if (parse_command_input(user_input, parsed_command, &data_value) < 0) {
      printf("Invalid input format. Type 'h' for help.\n");
      continue;
    }

    const MessageMapping *selected_command = NULL;
    for (size_t i = 0; i < MESSAGE_TABLE_SIZE; i++) {
      if (strcmp(message_table[i].command, parsed_command) == 0) {
        selected_command = &message_table[i];
        break;
      }
    }

    if (selected_command) {
      if (build_message(selected_command, tx_buffer, data_value) < 0) {
        continue;
      }

      if (write_bytes(i2c_fd, tx_buffer, TX_BUFFER_SIZE) < 0) {
        cleanup_and_exit(EXIT_FAILURE);
      }

      usleep(MESSAGE_DELAY_US);

      if (read_bytes(i2c_fd, rx_buffer, RX_BUFFER_SIZE) < 0) {
        cleanup_and_exit(EXIT_FAILURE);
      }

    } else {
      printf("Invalid command '%s'. Type 'h' for help.\n", parsed_command);
    }
  }

  cleanup_and_exit(EXIT_SUCCESS);
}
