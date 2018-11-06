#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>

#include <sys/mman.h>

#define FB "/dev/fb1"
#define PORT "/dev/ttyUSB0"

uint8_t *fb;
int serial;

#define BLOCKS 2400
#define    BOTHER 0010000

const int baud = 2000000;


uint32_t checksums[BLOCKS] = {0};

uint16_t getPixel(int x, int y) {
    int off = y * 480 + x;
    uint8_t r = fb[off * 4 + 2];
    uint8_t g = fb[off * 4 + 1];
    uint8_t b = fb[off * 4];

    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint32_t getBlockChecksum(int id) {
    int x = (id % 60) * 8;
    int y = (id / 60) * 8;

    uint32_t cs = 0;

    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            uint16_t p = getPixel(x + px, y + py);
            cs += p;
        }
    }
    return cs;
}

void sendBlock(int id) {
    int x = (id % 60) * 8;
    int y = (id / 60) * 8;

    uint8_t header[6];
    header[0] = 0x16;   // SOF
    header[1] = 0x16;   // SOF
    header[2] = 0x01;   // SOH
    header[3] = id >> 8;    // IDHIGH
    header[4] = id;     // IDLOW
    header[5] = 0x02;   // EOH
    write(serial, header, 6);
    uint16_t data[64];
    int i = 0;
    for (int py = 0; py < 8; py++) {
        for (int px = 0; px < 8; px++) {
            data[i++] = getPixel(x + px, y + py);
        }
    }
    write(serial, data, 128);
    header[0] = 0x04;   // EOT
    write(serial, header, 1);
}

struct termios _savedOptions;

void finish() {
    tcsetattr(serial, TCSANOW, &_savedOptions);
}

int main(int argc, char **argv) {

    system("fbset -fb " FB " -xres 480 -yres 320 -depth 32");

    serial = open(PORT, O_RDWR|O_NOCTTY);

    struct termios options;

    fcntl(serial, F_SETFL, 0);
    tcgetattr(serial, &_savedOptions);
    tcgetattr(serial, &options);
    cfmakeraw(&options);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~CRTSCTS;
    options.c_cflag &= ~CBAUD;
    if (strcmp(PORT, "/dev/tty") == 0) {
        options.c_lflag |= ISIG;
    }
    options.c_cflag |= BOTHER;
    options.c_ispeed = baud;
    options.c_ospeed = baud;
    if (tcsetattr(serial, TCSANOW, &options) != 0) {
        fprintf(stderr, "Can't set up serial\n");
    }

    atexit(finish);

    sleep(5);
    
    int fd = open(FB, O_RDWR);

    if (fd <= 0) {
        fprintf(stderr, "Unable to open " FB "\n");
        return -1;
    }

    fb = (uint8_t *)mmap(NULL, (480 * 320 * 4), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FILE, fd, 0);
    if (fb == NULL) {
        fprintf(stderr, "Unable to map " FB "\n");
        return -1;
    }


    for (int i = 0; i < BLOCKS; i++) {
        sendBlock(i);
    }

    while(1) {
        usleep(10000);
        for (int block = 0; block < BLOCKS; block++) {
            uint32_t cs = getBlockChecksum(block);
            if (checksums[block] != cs) {
                checksums[block] = cs;
                sendBlock(block);
            }
        }
    }

    return 0;
}
