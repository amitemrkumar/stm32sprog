#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

static const char *DEFAULT_DEV_NAME = "/dev/ttyUSB0";
static const int MAX_RETRIES = 10;

static const size_t MAX_BLOCK_SIZE = 256;

static const uint8_t ACK = 0x79;
typedef enum {
    CMD_GET_VERSION = 0x00,
    CMD_GET_READ_STATUS = 0x01,
    CMD_GET_ID = 0x02,
    CMD_READ_MEM = 0x11,
    CMD_GO = 0x21,
    CMD_WRITE_MEM = 0x31,
    CMD_ERASE = 0x43,
    CMD_EXTENDED_ERASE = 0x44,
    CMD_WRITE_PROTECT = 0x63,
    CMD_WRITE_UNPROTECT = 0x73,
    CMD_READ_PROTECT = 0x82,
    CMD_READ_UNPROTECT = 0x92
} Command;
#define NUM_COMMANDS_KNOWN 12

enum {
    ID_LOW_DENSITY = 0x0412,
    ID_MED_DENSITY = 0x0410,
    ID_HI_DENSITY = 0x0414,
    ID_CONNECTIVITY = 0x0418,
    ID_VALUE = 0x0420
};

typedef struct {
    uint8_t bootloaderVer;
    bool commands[NUM_COMMANDS_KNOWN];
    uint32_t flashBeginAddr;
    uint32_t flashEndAddr;
    int flashPagesPerSector;
    size_t flashPageSize;
    useconds_t eraseDelay;
    useconds_t writeDelay;
} DeviceParameters;

static void printUsage(void);

static bool openDevice(const char *devName, speed_t baud);
static bool bRead(int fd, uint8_t *buffer, size_t n);
static bool bWrite(int fd, const uint8_t *buffer, size_t n);

static bool stmConnect(void);

static int cmdIndex(uint8_t cmd);
static bool cmdSupported(Command cmd);

static bool stmRecvAck(void);
static bool stmSendByte(uint8_t byte);
static bool stmSendAddr(uint32_t addr);
static bool stmSendBlock(const uint8_t *buffer, size_t size);

static bool stmGetDevParams(void);
static bool stmEraseFlash(void);
static bool stmWriteBlock(uint32_t addr, const uint8_t *buff, size_t size);
static bool stmWriteFromFile(const char *fileName);
static bool stmReadBlock(uint32_t addr, uint8_t *buff, size_t size);
static bool stmCompareToFile(const char *fileName);
static bool stmRun();
static void printProgressBar(int percent);

static int ttyFd = -1;
static struct termios ttyOpts;
static DeviceParameters devParams;

int main(int argc, char **argv) {
    bool success = true;
    int opt;
    speed_t baud = B115200;
    char *devName = NULL;
    char *fileName = NULL;
    bool erase = false;
    bool verify = false;
    bool run = false;

    while((opt = getopt(argc, argv, "b:d:ehrvw:")) != -1) {
        switch(opt) {
        case 'b':
            switch(atoi(optarg)) {
            case 1200:
                baud = B1200;
                break;
            case 1800:
                baud = B1800;
                break;
            case 2400:
                baud = B2400;
                break;
            case 4800:
                baud = B4800;
                break;
            case 9600:
                baud = B9600;
                break;
            case 19200:
                baud = B19200;
                break;
            case 38400:
                baud = B38400;
                break;
            case 57600:
                baud = B57600;
                break;
            case 115200:
                baud = B115200;
                break;
            case 230400:
                baud = B230400;
                break;
            default:
                fprintf(stderr, "Baud rate \"%s\" is not supported.\n",
                        optarg);
                success = false;
                goto ExitApp;
            }
            break;
        case 'd':
            devName = strdup(optarg);
            break;
        case 'e':
            erase = true;
            break;
        case 'r':
            run = true;
            break;
        case 'v':
            verify = true;
            break;
        case 'w':
            fileName = strdup(optarg);
            break;
        case 'h':
        default:
            printUsage();
            goto ExitApp;
        }
    }

    success = optind == argc;
    if(!success) {
        fprintf(stderr, "Too many arguments.\n");
        printUsage();
        goto ExitApp;
    }

    success = erase || run || fileName != NULL;
    if(!success) {
        fprintf(stderr, "No actions specified.\n");
        printUsage();
        goto ExitApp;
    }

    success = fileName || !verify;
    if(!success) {
        fprintf(stderr, "Verification requires write.\n");
        printUsage();
        goto ExitApp;
    }

    /**************************************/

    success = openDevice(devName ? devName : DEFAULT_DEV_NAME, baud);
    if(!success) goto ExitApp;

    success = stmConnect();
    if(!success) {
        fprintf(stderr, "STM32 not detected.\n");
        goto ExitApp;
    }

    success = stmGetDevParams();
    if(!success) {
        fprintf(stderr, "Device not supported.\n");
        goto ExitApp;
    }
    int major = devParams.bootloaderVer >> 4;
    int minor = devParams.bootloaderVer & 0x0F;
    printf("Bootloader version %d.%d detected.\n", major, minor);

    if(erase) {
        success = stmEraseFlash();
        if(!success) {
            fprintf(stderr, "Unable to erase flash.\n");
            goto ExitApp;
        }
    }

    if(fileName != NULL) {
        success = stmWriteFromFile(fileName);
        if(!success) {
            fprintf(stderr, "Unable to write flash.\n");
            goto ExitApp;
        }
        if(verify) {
            success = stmCompareToFile(fileName);
            if(!success) {
                fprintf(stderr, "Flash verification failed.\n");
                goto ExitApp;
            }
        }
    }

    if(run) {
        success = stmRun();
        if(!success) {
            fprintf(stderr, "Unable to start firmware.\n");
            goto ExitApp;
        }
    }

ExitApp:
    if(ttyFd != -1) close(ttyFd);
    free(devName);
    free(fileName);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void printUsage(void) {
    fprintf(stderr,
            "Usage: stm32sprog OPTIONS\n"
            "\n"
            "OPTIONS:\n"
            "  -b BAUD    Set the baud rate. (115200)\n"
            "  -d DEVICE  Communicate using DEVICE. (/dev/ttyUSB0)\n"
            "  -e         Erase the target device.\n"
            "  -h         Print this help.\n"
            "  -r         Run the firmware on the device.\n"
            "  -v         Verify the write process.\n"
            "  -w FILE    Write the raw binary FILE to the target device.\n"
            "\n");
}

static bool openDevice(const char *devName, speed_t baud) {
    ttyFd = open(devName, O_RDWR | O_NOCTTY);
    if(ttyFd == -1) {
        fprintf(stderr, "Unable to open device \"%s\"\n", devName);
        goto DeviceOpenError;
    }

    cfmakeraw(&ttyOpts);
    ttyOpts.c_cflag &= ~CSTOPB;
    ttyOpts.c_cflag |= PARENB;
    ttyOpts.c_cflag &= ~PARODD;
    ttyOpts.c_cc[VMIN] = 0;
    ttyOpts.c_cc[VTIME] = 5;
    if(cfsetspeed(&ttyOpts, baud) == -1) {
        fprintf(stderr, "Unable to set baud rate.\n");
        goto DeviceOptionsError;
    } else if(tcsetattr(ttyFd, TCSANOW, &ttyOpts) == -1) {
        fprintf(stderr, "Unable to set device options.\n");
        goto DeviceOptionsError;
    }

    return true;

DeviceOptionsError:
    close(ttyFd);
    ttyFd = -1;
DeviceOpenError:
    return false;
}

static bool bRead(int fd, uint8_t *buffer, size_t n) {
    while(n) {
        ssize_t result = read(fd, buffer, n);
        if(result > 0) {
            buffer += result;
            n -= result;
        } else if(result < 0) {
            fprintf(stderr, "Read error.\n");
            return false;
        }
    }
    return true;
}

static bool bWrite(int fd, const uint8_t *buffer, size_t n) {
    while(n) {
        ssize_t result = write(fd, buffer, n);
        if(result > 0) {
            buffer += result;
            n -= result;
        } else if(result < 0) {
            fprintf(stderr, "Write error.\n");
            return false;
        }
    }
    return true;
}

static bool stmConnect(void) {
    int status;
    ioctl(ttyFd, TIOCMGET, &status);
    status |= TIOCM_DTR;
    ioctl(ttyFd, TIOCMSET, &status);
    usleep(10000);
    status &= ~TIOCM_DTR;
    ioctl(ttyFd, TIOCMSET, &status);
    usleep(10000);

    uint8_t data = 0x7F;
    int retries = 0;
    do {
        if(++retries > MAX_RETRIES) return false;
        (void)bWrite(ttyFd, &data, 1);
    } while(!stmRecvAck());
    return true;
}

static int cmdIndex(uint8_t cmd) {
    int idx = -1;
    switch(cmd) {
    case CMD_GET_VERSION:     idx++;
    case CMD_GET_READ_STATUS: idx++;
    case CMD_GET_ID:          idx++;
    case CMD_READ_MEM:        idx++;
    case CMD_GO:              idx++;
    case CMD_WRITE_MEM:       idx++;
    case CMD_ERASE:           idx++;
    case CMD_EXTENDED_ERASE:  idx++;
    case CMD_WRITE_PROTECT:   idx++;
    case CMD_WRITE_UNPROTECT: idx++;
    case CMD_READ_PROTECT:    idx++;
    case CMD_READ_UNPROTECT:  idx++;
    default:                  break;
    }
    assert(idx < NUM_COMMANDS_KNOWN);
    return idx;
}

static bool cmdSupported(Command cmd) {
    int idx = cmdIndex(cmd);
    assert(idx >= 0);
    return devParams.commands[idx];
}

static bool stmRecvAck(void) {
    uint8_t data = 0;
    if(!bRead(ttyFd, &data, 1)) return false;
    return data == ACK;
}

static bool stmSendByte(uint8_t byte) {
    uint8_t buffer[] = { byte, ~byte };
    if(!bWrite(ttyFd, buffer, sizeof(buffer))) return false;
    return stmRecvAck();
}

static bool stmSendAddr(uint32_t addr) {
    assert(addr % 4 == 0);
    uint8_t buffer[5] = { 0 };
    for(int i = 0; i < 4; ++i) {
        buffer[i] = (uint8_t)(addr >> ((3 - i) * CHAR_BIT));
        buffer[4] ^= buffer[i];
    }
    if(!bWrite(ttyFd, buffer, sizeof(buffer))) return false;
    return stmRecvAck();
}

static bool stmSendBlock(const uint8_t *buffer, size_t size) {
    assert(size > 0 && size <= MAX_BLOCK_SIZE);
    size_t padding = (4 - (size % 4)) % 4;
    uint8_t n = size + padding - 1;
    uint8_t checksum = n;
    for(size_t i = 0; i < size; ++i) checksum ^= buffer[i];
    if(!bWrite(ttyFd, &n, 1)) return false;
    if(!bWrite(ttyFd, buffer, size)) return false;
    for(size_t i = 0; i < padding; ++i) {
        uint8_t data = 0xFF;
        checksum ^= data;
        if(!bWrite(ttyFd, &data, 1)) return false;
    }
    if(!bWrite(ttyFd, &checksum, 1)) return false;
    return stmRecvAck();
}

static bool stmGetDevParams(void) {
    uint8_t data = 0;

    devParams.flashBeginAddr = 0x08000000;
    devParams.flashEndAddr = 0x08008000;
    devParams.flashPagesPerSector = 4;
    devParams.flashPageSize = 1024;
    devParams.eraseDelay = 40000;
    devParams.writeDelay = 80000;

    if(!stmSendByte(CMD_GET_VERSION)) return false;
    if(!bRead(ttyFd, &data, 1)) return false;
    if(!bRead(ttyFd, &devParams.bootloaderVer, 1)) return false;
    for(int i = 0; i < NUM_COMMANDS_KNOWN; ++i) devParams.commands[i] = false;
    for(int i = data; i > 0; --i) {
        if(!bRead(ttyFd, &data, 1)) return false;
        int idx = cmdIndex(data);
        if(idx >= 0) devParams.commands[idx] = true;
    }
    if(!stmRecvAck()) return false;

    if(!cmdSupported(CMD_GET_ID)) {
        fprintf(stderr, "Target device does not support GET_ID command.\n");
        return false;
    }
    if(!stmSendByte(CMD_GET_ID)) return false;
    if(!bRead(ttyFd, &data, 1)) return false;
    if(data != 1) return false;
    uint16_t id = 0;
    for(int i = data; i >= 0; --i) {
        if(!bRead(ttyFd, &data, 1)) return false;
        if(i < 2) {
            id |= data << (i * CHAR_BIT);
        }
    }
    if(!stmRecvAck()) return false;
    switch(id) {
    case ID_LOW_DENSITY:
        devParams.flashEndAddr = 0x08008000;
        break;
    case ID_MED_DENSITY:
        devParams.flashEndAddr = 0x08020000;
        break;
    case ID_HI_DENSITY:
        devParams.flashEndAddr = 0x08080000;
        devParams.flashPagesPerSector = 2;
        devParams.flashPageSize = 2048;
        break;
    case ID_CONNECTIVITY:
        devParams.flashEndAddr = 0x08040000;
        devParams.flashPagesPerSector = 2;
        devParams.flashPageSize = 2048;
        break;
    case ID_VALUE:
        devParams.flashEndAddr = 0x08020000;
        break;
    default:
        return false;
    }

    return true;
}

static bool stmEraseFlash(void) {
    if(cmdSupported(CMD_ERASE)) {
        if(!stmSendByte(CMD_ERASE)) return false;
        if(!stmSendByte(0xFF)) return false;
    } else if(cmdSupported(CMD_EXTENDED_ERASE)) {
        printf("The extended erase command has not been tested with an actual "
                    "device.  If you\n"
                "choose to continue, please provide feedback so I can fix any "
                    "bugs and remove\n"
                "this message.\n"
                "\n"
                "Continue? (y/N): ");
        int c;
        do {
            c = getchar();
            if(c == 'n' || c == 'N' || c == '\n') {
                printf("\n");
                return false;
            }
        } while(c != 'y' && c != 'Y');
        printf("\n");

        // TODO: Test this code with an actual device.
        if(!stmSendByte(CMD_EXTENDED_ERASE)) return false;
        uint8_t data[] = { 0xFF, 0xFF, 0x00 };
        if(!bWrite(ttyFd, data, sizeof(data))) return false;
        if(!stmRecvAck()) return false;
    } else {
        fprintf(stderr,
                "Target device does not support known erase commands.\n");
        return false;
    }

    useconds_t delay = (devParams.eraseDelay / 100) + 1;
    printf("Erasing:\n");
    for(int i = 1; i <= 100; ++i) {
        usleep(delay);
        printProgressBar(i);
    }
    printf("\n");
    return true;
}

static bool stmWriteBlock(uint32_t addr, const uint8_t *buff, size_t size) {
    if(!stmSendByte(CMD_WRITE_MEM)) return false;
    if(!stmSendAddr(addr)) return false;
    if(!stmSendBlock(buff, size)) return false;
    return true;
}

static bool stmWriteFromFile(const char *fileName) {
    if(!cmdSupported(CMD_WRITE_MEM)) {
        fprintf(stderr,
                "Target device does not support known write commands.\n");
        return false;
    }

    FILE *firmware = fopen(fileName, "rb");
    if(firmware == NULL) {
        fprintf(stderr, "Error opening file \"%s\"\n", fileName);
        return false;
    }
    (void)fseek(firmware, 0L, SEEK_END);
    long fileEnd = ftell(firmware);
    rewind(firmware);

    printf("Writing:\n");

    uint8_t buff[MAX_BLOCK_SIZE];
    uint32_t addr = devParams.flashBeginAddr;
    size_t i = 0;
    int c;
    bool ok = true;
    long bytesWritten = 0;
    while(ok && (c = fgetc(firmware)) != EOF) {
        buff[i] = (uint8_t)c;
        ++i;
        if(i == MAX_BLOCK_SIZE) {
            ok = stmWriteBlock(addr, buff, i);
            usleep(devParams.writeDelay);
            addr += i;
            bytesWritten += i;
            printProgressBar(bytesWritten * 100 / fileEnd);
            i = 0;
        }
    }
    if(ok && i > 0) {
        ok = stmWriteBlock(addr, buff, i);
        usleep(devParams.writeDelay);
        bytesWritten += i;
        printProgressBar(bytesWritten * 100 / fileEnd);
    }
    printf("\n");

    fclose(firmware);
    return ok;
}

static bool stmReadBlock(uint32_t addr, uint8_t *buff, size_t size) {
    if(!stmSendByte(CMD_READ_MEM)) return false;
    if(!stmSendAddr(addr)) return false;
    if(!stmSendByte(size - 1)) return false;
    return bRead(ttyFd, buff, size);
}

static bool stmCompareToFile(const char *fileName) {
    if(!cmdSupported(CMD_READ_MEM)) {
        fprintf(stderr,
                "Target device does not support known read commands.\n");
        return false;
    }

    FILE *firmware = fopen(fileName, "rb");
    if(firmware == NULL) {
        fprintf(stderr, "Error opening file \"%s\"\n", fileName);
        return false;
    }
    (void)fseek(firmware, 0L, SEEK_END);
    long fileEnd = ftell(firmware);
    rewind(firmware);

    printf("Verifying:\n");

    uint8_t fileBuff[MAX_BLOCK_SIZE];
    uint8_t firmwareBuff[MAX_BLOCK_SIZE];
    uint32_t addr = devParams.flashBeginAddr;
    size_t i = 0;
    int c;
    bool ok = true;
    long bytesRead = 0;

    while(ok && (c = fgetc(firmware)) != EOF) {
        fileBuff[i] = (uint8_t)c;
        ++i;
        if(i == MAX_BLOCK_SIZE) {
            ok = stmReadBlock(addr, firmwareBuff, i);
            if(ok) ok = (memcmp(fileBuff, firmwareBuff, i) == 0);
            addr += i;
            bytesRead += i;
            printProgressBar(bytesRead * 100 / fileEnd);
            i = 0;
        }
    }
    if(ok && i > 0) {
        ok = stmReadBlock(addr, firmwareBuff, i);
        if(ok) ok = (memcmp(fileBuff, firmwareBuff, i) == 0);
        bytesRead += i;
        printProgressBar(bytesRead * 100 / fileEnd);
    }
    printf("\n");

    fclose(firmware);
    return ok;
}

static bool stmRun() {
    if(!stmSendByte(CMD_GO)) return false;
    return stmSendAddr(devParams.flashBeginAddr);
}

static void printProgressBar(int percent) {
    int num = percent * 70 / 100;
    printf("\r%3d%%[", percent);
    for(int i = 0; i < 70; ++i) {
        printf(i < num ? "=" : " ");
    }
    printf("]");
    fflush(stdout);
}

