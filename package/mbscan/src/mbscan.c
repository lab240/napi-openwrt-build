/*
 * mbscan — Modbus RTU bus scanner
 *
 * Opens a serial port once and scans a range of slave addresses
 * by sending a Modbus RTU Read Holding Registers (FC03) request.
 * Reports responding devices with register values.
 *
 * No external dependencies — pure POSIX + own CRC16 implementation.
 *
 * Usage: mbscan -p /dev/ttyUSB0 [-b 115200] [-d 8N1] [-f 1] [-t 247]
 *              [-o 100] [-r 1] [-c 1]
 *
 * (c) 2025 NapiLab, GPL-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/select.h>
#include <getopt.h>
#include <stdint.h>

/* ---- Modbus CRC16 ---- */

static const uint16_t crc_table[256] = {
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

static uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc_table[(crc ^ data[i]) & 0xFF];
    }
    return crc;
}

/* ---- Serial port ---- */

static speed_t baud_to_speed(int baud)
{
    switch (baud) {
        case 1200:   return B1200;
        case 2400:   return B2400;
        case 4800:   return B4800;
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:
            fprintf(stderr, "Unsupported baud rate: %d\n", baud);
            return B115200;
    }
}

static int open_serial(const char *port, int baud, char parity, int databits, int stopbits)
{
    int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", port, strerror(errno));
        return -1;
    }

    struct termios tio;
    memset(&tio, 0, sizeof(tio));

    tio.c_cflag = CLOCAL | CREAD;

    /* Data bits */
    switch (databits) {
        case 7: tio.c_cflag |= CS7; break;
        default: tio.c_cflag |= CS8; break;
    }

    /* Parity */
    switch (parity) {
        case 'E': case 'e':
            tio.c_cflag |= PARENB;
            break;
        case 'O': case 'o':
            tio.c_cflag |= PARENB | PARODD;
            break;
        default: /* None */
            break;
    }

    /* Stop bits */
    if (stopbits == 2)
        tio.c_cflag |= CSTOPB;

    /* Baud rate */
    speed_t sp = baud_to_speed(baud);
    cfsetispeed(&tio, sp);
    cfsetospeed(&tio, sp);

    /* Raw mode */
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 0;

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &tio) < 0) {
        fprintf(stderr, "tcsetattr failed: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    /* Clear non-blocking for normal operation with select() */
    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    return fd;
}

/* ---- Modbus RTU request/response ---- */

/*
 * Build FC03 (Read Holding Registers) request frame.
 * Returns frame length (always 8).
 */
static int build_fc03_request(uint8_t *buf, uint8_t slave, uint16_t start_reg, uint16_t count)
{
    buf[0] = slave;
    buf[1] = 0x03;                    /* FC03 */
    buf[2] = (start_reg >> 8) & 0xFF; /* Start register high */
    buf[3] = start_reg & 0xFF;        /* Start register low */
    buf[4] = (count >> 8) & 0xFF;     /* Count high */
    buf[5] = count & 0xFF;            /* Count low */

    uint16_t crc = modbus_crc16(buf, 6);
    buf[6] = crc & 0xFF;              /* CRC low */
    buf[7] = (crc >> 8) & 0xFF;       /* CRC high */

    return 8;
}

/*
 * Wait for response with timeout (milliseconds).
 * Returns bytes read, 0 on timeout, -1 on error.
 */
static int read_response(int fd, uint8_t *buf, size_t bufsize, int timeout_ms)
{
    int total = 0;
    struct timeval start, now;
    gettimeofday(&start, NULL);

    while (total < (int)bufsize) {
        gettimeofday(&now, NULL);
        int elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                      (now.tv_usec - start.tv_usec) / 1000;
        int remaining = timeout_ms - elapsed;
        if (remaining <= 0)
            break;

        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        struct timeval tv;
        tv.tv_sec  = remaining / 1000;
        tv.tv_usec = (remaining % 1000) * 1000;

        int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (ret == 0)
            break; /* timeout */

        int n = read(fd, buf + total, bufsize - total);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return -1;
        }
        if (n == 0)
            break;

        total += n;

        /*
         * For FC03 response: slave(1) + fc(1) + byte_count(1) + data(N) + crc(2)
         * Once we have at least 5 bytes, we know the expected length.
         */
        if (total >= 5) {
            int expected;
            if (buf[1] & 0x80) {
                /* Exception response: slave + fc|0x80 + exception_code + crc(2) = 5 */
                expected = 5;
            } else {
                /* Normal: 3 + byte_count + 2 */
                expected = 3 + buf[2] + 2;
            }
            if (total >= expected)
                break;
        }
    }

    return total;
}

/*
 * Validate response CRC and structure.
 * Returns 1 if valid FC03 response, 0 otherwise.
 */
static int validate_response(const uint8_t *buf, int len, uint8_t slave)
{
    if (len < 5)
        return 0;

    /* Check slave address */
    if (buf[0] != slave)
        return 0;

    /* Check CRC */
    uint16_t crc_calc = modbus_crc16(buf, len - 2);
    uint16_t crc_recv = buf[len - 2] | (buf[len - 1] << 8);
    if (crc_calc != crc_recv)
        return 0;

    /* Exception? */
    if (buf[1] & 0x80)
        return 0;

    /* FC03 response */
    if (buf[1] != 0x03)
        return 0;

    return 1;
}

/* ---- Inter-frame delay ---- */

/*
 * Modbus RTU requires 3.5 character times of silence between frames.
 * At 9600 baud: 3.5 * 11 bits / 9600 = ~4 ms
 * At 115200 baud: ~0.33 ms
 * We use a minimum of 2 ms for safety.
 */
static void inter_frame_delay(int baud)
{
    int delay_us;
    if (baud <= 19200)
        delay_us = 3500 * 11 * 1000 / baud; /* 3.5 char times */
    else
        delay_us = 2000; /* 2 ms minimum */

    usleep(delay_us);
}

/* ---- Usage ---- */

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s -p PORT [options]\n"
        "\n"
        "Modbus RTU bus scanner. Sends FC03 to each address and reports responses.\n"
        "\n"
        "Options:\n"
        "  -p PORT    Serial port (required, e.g. /dev/ttyUSB0)\n"
        "  -b BAUD    Baud rate (default: 115200)\n"
        "  -d PARAMS  Data format: 8N1, 8E1, 8O1, 7E1, etc. (default: 8N1)\n"
        "  -f FROM    Start address (default: 1)\n"
        "  -t TO      End address (default: 247)\n"
        "  -o MS      Timeout per address in ms (default: 100)\n"
        "  -r REG     Start register, 0-based (default: 0)\n"
        "  -c COUNT   Number of registers to read (default: 1)\n"
        "  -v         Verbose output\n"
        "  -h         Show this help\n"
        "\n"
        "Examples:\n"
        "  %s -p /dev/ttyUSB0\n"
        "  %s -p /dev/ttyS1 -b 9600 -d 8E1 -f 1 -t 30 -o 200\n"
        "\n",
        prog, prog, prog);
}

/* ---- Main ---- */

int main(int argc, char *argv[])
{
    const char *port = NULL;
    int baud     = 115200;
    char parity  = 'N';
    int databits = 8;
    int stopbits = 1;
    int from     = 1;
    int to       = 247;
    int timeout  = 100;   /* ms */
    int start_reg = 0;    /* 0-based */
    int count    = 1;
    int verbose  = 0;

    int opt;
    while ((opt = getopt(argc, argv, "p:b:d:f:t:o:r:c:vh")) != -1) {
        switch (opt) {
        case 'p': port = optarg; break;
        case 'b': baud = atoi(optarg); break;
        case 'd':
            /* Parse format like "8N1", "8E1", "7E1" */
            if (strlen(optarg) >= 3) {
                databits = optarg[0] - '0';
                parity   = optarg[1];
                stopbits = optarg[2] - '0';
            }
            break;
        case 'f': from = atoi(optarg); break;
        case 't': to = atoi(optarg); break;
        case 'o': timeout = atoi(optarg); break;
        case 'r': start_reg = atoi(optarg); break;
        case 'c': count = atoi(optarg); break;
        case 'v': verbose = 1; break;
        case 'h': usage(argv[0]); return 0;
        default:  usage(argv[0]); return 1;
        }
    }

    if (!port) {
        fprintf(stderr, "Error: -p PORT is required\n\n");
        usage(argv[0]);
        return 1;
    }

    if (from < 1) from = 1;
    if (to > 247) to = 247;
    if (from > to) {
        fprintf(stderr, "Error: FROM (%d) > TO (%d)\n", from, to);
        return 1;
    }
    if (count < 1) count = 1;
    if (count > 125) count = 125;

    /* Open serial port */
    int fd = open_serial(port, baud, parity, databits, stopbits);
    if (fd < 0)
        return 1;

    int total = to - from + 1;
    int found = 0;

    fprintf(stderr, "mbscan: scanning %s %d-%c%c%c, addresses %d-%d, timeout %dms\n",
            port, baud, '0' + databits, parity, '0' + stopbits, from, to, timeout);
    fprintf(stderr, "mbscan: reading %d register(s) starting at %d\n\n", count, start_reg);

    for (int addr = from; addr <= to; addr++) {
        int progress = addr - from + 1;

        if (verbose)
            fprintf(stderr, "[%d/%d] Trying address %d...\n", progress, total, addr);
        else
            fprintf(stderr, "\r[%d/%d] Scanning address %d...    ", progress, total, addr);

        /* Flush any stale data */
        tcflush(fd, TCIOFLUSH);

        /* Build and send request */
        uint8_t req[8];
        build_fc03_request(req, (uint8_t)addr, (uint16_t)start_reg, (uint16_t)count);

        int written = write(fd, req, 8);
        if (written != 8) {
            fprintf(stderr, "\nWrite error at address %d: %s\n", addr, strerror(errno));
            continue;
        }

        /* Wait for response to finish transmitting */
        tcdrain(fd);

        /* Read response */
        uint8_t resp[256];
        int rlen = read_response(fd, resp, sizeof(resp), timeout);

        if (rlen > 0 && validate_response(resp, rlen, (uint8_t)addr)) {
            found++;
            int byte_count = resp[2];
            int nregs = byte_count / 2;

            fprintf(stderr, "\n");
            printf("Found slave %3d:", addr);

            for (int i = 0; i < nregs && i < count; i++) {
                uint16_t val = (resp[3 + i * 2] << 8) | resp[3 + i * 2 + 1];
                printf(" [%d]=%u", start_reg + i, val);
            }
            printf("\n");
            fflush(stdout);
        } else if (verbose && rlen > 0) {
            fprintf(stderr, " -- got %d bytes but invalid response\n", rlen);
        }

        /* Inter-frame delay */
        inter_frame_delay(baud);
    }

    fprintf(stderr, "\n\nmbscan: done. Found %d device(s).\n", found);

    close(fd);
    return found > 0 ? 0 : 1;
}
