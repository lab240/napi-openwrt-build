/*
 * mbscan — Modbus RTU bus scanner
 *
 * Opens a serial port once and scans a range of slave addresses
 * by sending a Modbus RTU Read Holding Registers (FC03) request.
 * Reports responding devices with register values.
 *
 * Cross-platform: Linux (POSIX termios) + Windows (Win32 API).
 * No external dependencies — pure C with own CRC16 implementation.
 *
 * Linux:   gcc -O2 -Wall -o mbscan mbscan.c
 * Windows: cl mbscan.c /Fe:mbscan.exe
 *          (or: gcc -O2 -Wall -o mbscan.exe mbscan.c on MSYS2/MinGW)
 *
 * (c) 2025 NapiLab, GPL-2.0
 */

#ifndef _WIN32
  #define _DEFAULT_SOURCE   /* usleep() on glibc */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <termios.h>
  #include <sys/time.h>
  #include <sys/select.h>
  #include <getopt.h>
#endif

/* ================================================================
 * Minimal getopt for Windows (MSVC has no getopt)
 * ================================================================ */

#ifdef _WIN32

static char *optarg = NULL;
static int   optind = 1;

static int getopt(int argc, char *const argv[], const char *optstring)
{
    if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
        return -1;

    char c = argv[optind][1];
    const char *p = strchr(optstring, c);
    if (!p)
        return '?';

    optind++;

    if (p[1] == ':') {
        if (optind >= argc) {
            fprintf(stderr, "Option -%c requires an argument\n", c);
            return '?';
        }
        optarg = argv[optind++];
    }

    return c;
}

#endif /* _WIN32 */


/* ================================================================
 * Portable time helpers
 * ================================================================ */

/* Returns current time in milliseconds (monotonic-ish) */
static uint64_t now_ms(void)
{
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
#endif
}

/* Format seconds as MM:SS into buf (must be >= 8 bytes) */
static void fmt_time(int secs, char *buf)
{
    if (secs < 0) secs = 0;
    sprintf(buf, "%02d:%02d", secs / 60, secs % 60);
}

static void sleep_ms(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}


/* ================================================================
 * Modbus CRC16 (platform-independent)
 * ================================================================ */

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

/* ================================================================
 * Modbus RTU request/response (platform-independent)
 * ================================================================ */

static int build_fc03_request(uint8_t *buf, uint8_t slave, uint16_t start_reg, uint16_t count)
{
    buf[0] = slave;
    buf[1] = 0x03;                    /* FC03 */
    buf[2] = (start_reg >> 8) & 0xFF;
    buf[3] = start_reg & 0xFF;
    buf[4] = (count >> 8) & 0xFF;
    buf[5] = count & 0xFF;

    uint16_t crc = modbus_crc16(buf, 6);
    buf[6] = crc & 0xFF;
    buf[7] = (crc >> 8) & 0xFF;

    return 8;
}

static int validate_response(const uint8_t *buf, int len, uint8_t slave)
{
    if (len < 5)
        return 0;
    if (buf[0] != slave)
        return 0;

    uint16_t crc_calc = modbus_crc16(buf, len - 2);
    uint16_t crc_recv = buf[len - 2] | (buf[len - 1] << 8);
    if (crc_calc != crc_recv)
        return 0;

    if (buf[1] & 0x80)
        return 0;
    if (buf[1] != 0x03)
        return 0;

    return 1;
}


/* ================================================================
 * Platform-specific serial port layer
 * ================================================================ */

#ifdef _WIN32

/* ---- Windows: Win32 serial API ---- */

typedef HANDLE serial_t;
#define SERIAL_INVALID INVALID_HANDLE_VALUE

static serial_t open_serial(const char *port, int baud, char parity, int databits, int stopbits)
{
    char devpath[64];
    if (strncmp(port, "\\\\.\\", 4) == 0 || strncmp(port, "//./", 4) == 0) {
        snprintf(devpath, sizeof(devpath), "%s", port);
    } else {
        snprintf(devpath, sizeof(devpath), "\\\\.\\%s", port);
    }

    HANDLE hComm = CreateFileA(
        devpath, GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL
    );

    if (hComm == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Cannot open %s: error %lu\n", port, GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(hComm, &dcb)) {
        fprintf(stderr, "GetCommState failed: error %lu\n", GetLastError());
        CloseHandle(hComm);
        return INVALID_HANDLE_VALUE;
    }

    dcb.BaudRate = baud;
    dcb.ByteSize = (BYTE)databits;

    switch (parity) {
        case 'E': case 'e': dcb.Parity = EVENPARITY; break;
        case 'O': case 'o': dcb.Parity = ODDPARITY;  break;
        default:            dcb.Parity = NOPARITY;    break;
    }

    dcb.StopBits = (stopbits == 2) ? TWOSTOPBITS : ONESTOPBIT;

    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl  = DTR_CONTROL_DISABLE;
    dcb.fRtsControl  = RTS_CONTROL_DISABLE;
    dcb.fOutX        = FALSE;
    dcb.fInX         = FALSE;
    dcb.fBinary      = TRUE;
    dcb.fParity      = (parity != 'N' && parity != 'n') ? TRUE : FALSE;

    if (!SetCommState(hComm, &dcb)) {
        fprintf(stderr, "SetCommState failed: error %lu\n", GetLastError());
        CloseHandle(hComm);
        return INVALID_HANDLE_VALUE;
    }

    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout         = MAXDWORD;
    timeouts.ReadTotalTimeoutMultiplier  = 0;
    timeouts.ReadTotalTimeoutConstant    = 0;
    timeouts.WriteTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant   = 1000;

    if (!SetCommTimeouts(hComm, &timeouts)) {
        fprintf(stderr, "SetCommTimeouts failed: error %lu\n", GetLastError());
        CloseHandle(hComm);
        return INVALID_HANDLE_VALUE;
    }

    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return hComm;
}

static int reconfig_serial(serial_t h, int baud, char parity, int databits, int stopbits)
{
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(h, &dcb))
        return -1;

    dcb.BaudRate = baud;
    dcb.ByteSize = (BYTE)databits;

    switch (parity) {
        case 'E': case 'e': dcb.Parity = EVENPARITY; break;
        case 'O': case 'o': dcb.Parity = ODDPARITY;  break;
        default:            dcb.Parity = NOPARITY;    break;
    }

    dcb.StopBits = (stopbits == 2) ? TWOSTOPBITS : ONESTOPBIT;
    dcb.fParity  = (parity != 'N' && parity != 'n') ? TRUE : FALSE;

    if (!SetCommState(h, &dcb))
        return -1;

    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return 0;
}

static void close_serial(serial_t h)
{
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
}

static void flush_serial(serial_t h)
{
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
}

static int write_serial(serial_t h, const uint8_t *data, int len)
{
    DWORD written = 0;
    if (!WriteFile(h, data, (DWORD)len, &written, NULL))
        return -1;
    FlushFileBuffers(h);
    return (int)written;
}

static int read_response(serial_t h, uint8_t *buf, size_t bufsize, int timeout_ms)
{
    int total = 0;
    ULONGLONG start = GetTickCount64();

    while (total < (int)bufsize) {
        ULONGLONG now = GetTickCount64();
        if ((int)(now - start) >= timeout_ms)
            break;

        DWORD nread = 0;
        if (!ReadFile(h, buf + total, (DWORD)(bufsize - total), &nread, NULL))
            return -1;

        if (nread > 0) {
            total += (int)nread;
        } else {
            Sleep(1);
        }

        if (total >= 5) {
            int expected;
            if (buf[1] & 0x80)
                expected = 5;
            else
                expected = 3 + buf[2] + 2;
            if (total >= expected)
                break;
        }
    }

    return total;
}

static void inter_frame_delay(int baud)
{
    int delay_ms;
    if (baud <= 19200)
        delay_ms = (3500 * 11) / baud + 1;
    else
        delay_ms = 2;
    Sleep(delay_ms);
}

#else

/* ---- Linux/POSIX: termios serial API ---- */

typedef int serial_t;
#define SERIAL_INVALID (-1)

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

static serial_t open_serial(const char *port, int baud, char parity, int databits, int stopbits)
{
    int fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "Cannot open %s: %s\n", port, strerror(errno));
        return -1;
    }

    struct termios tio;
    memset(&tio, 0, sizeof(tio));

    tio.c_cflag = CLOCAL | CREAD;

    switch (databits) {
        case 7: tio.c_cflag |= CS7; break;
        default: tio.c_cflag |= CS8; break;
    }

    switch (parity) {
        case 'E': case 'e': tio.c_cflag |= PARENB; break;
        case 'O': case 'o': tio.c_cflag |= PARENB | PARODD; break;
        default: break;
    }

    if (stopbits == 2)
        tio.c_cflag |= CSTOPB;

    speed_t sp = baud_to_speed(baud);
    cfsetispeed(&tio, sp);
    cfsetospeed(&tio, sp);

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

    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    return fd;
}

static int reconfig_serial(serial_t fd, int baud, char parity, int databits, int stopbits)
{
    struct termios tio;
    memset(&tio, 0, sizeof(tio));

    tio.c_cflag = CLOCAL | CREAD;

    switch (databits) {
        case 7: tio.c_cflag |= CS7; break;
        default: tio.c_cflag |= CS8; break;
    }

    switch (parity) {
        case 'E': case 'e': tio.c_cflag |= PARENB; break;
        case 'O': case 'o': tio.c_cflag |= PARENB | PARODD; break;
        default: break;
    }

    if (stopbits == 2)
        tio.c_cflag |= CSTOPB;

    speed_t sp = baud_to_speed(baud);
    cfsetispeed(&tio, sp);
    cfsetospeed(&tio, sp);

    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VMIN]  = 0;
    tio.c_cc[VTIME] = 0;

    tcflush(fd, TCIOFLUSH);
    if (tcsetattr(fd, TCSANOW, &tio) < 0)
        return -1;

    return 0;
}

static void close_serial(serial_t fd)
{
    if (fd >= 0)
        close(fd);
}

static void flush_serial(serial_t fd)
{
    tcflush(fd, TCIOFLUSH);
}

static int write_serial(serial_t fd, const uint8_t *data, int len)
{
    int written = write(fd, data, len);
    if (written > 0)
        tcdrain(fd);
    return written;
}

static int read_response(serial_t fd, uint8_t *buf, size_t bufsize, int timeout_ms)
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
            break;

        int n = read(fd, buf + total, bufsize - total);
        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN) continue;
            return -1;
        }
        if (n == 0)
            break;

        total += n;

        if (total >= 5) {
            int expected;
            if (buf[1] & 0x80)
                expected = 5;
            else
                expected = 3 + buf[2] + 2;
            if (total >= expected)
                break;
        }
    }

    return total;
}

static void inter_frame_delay(int baud)
{
    int delay_us;
    if (baud <= 19200)
        delay_us = 3500 * 11 * 1000 / baud;
    else
        delay_us = 2000;
    usleep(delay_us);
}

#endif /* _WIN32 / POSIX */


/* ================================================================
 * Scan results storage
 * ================================================================ */

#define MAX_FOUND 247
#define MAX_REGS  125

typedef struct {
    const char *port;
    int addr;
    int baud;
    int databits;
    char parity;
    int stopbits;
    uint16_t regs[MAX_REGS];
    int nregs;
    int start_reg;
} found_device_t;

static found_device_t found_devices[MAX_FOUND];
static int found_count = 0;


/* ================================================================
 * Passive line sniffer — called when write errors detected
 * ================================================================ */

static const char *passive_sniff(serial_t ser, int listen_ms)
{
    uint8_t buf[256];
    int total = 0;

#ifdef _WIN32
    ULONGLONG t0 = GetTickCount64();
    while (total < (int)sizeof(buf)) {
        if ((int)(GetTickCount64() - t0) >= listen_ms) break;
        DWORD nr = 0;
        ReadFile(ser, buf + total, (DWORD)(sizeof(buf) - total), &nr, NULL);
        if (nr > 0) total += (int)nr; else Sleep(10);
    }
#else
    struct timeval t0, tn;
    gettimeofday(&t0, NULL);
    while (total < (int)sizeof(buf)) {
        gettimeofday(&tn, NULL);
        int rem = listen_ms - (int)((tn.tv_sec - t0.tv_sec) * 1000 +
                                    (tn.tv_usec - t0.tv_usec) / 1000);
        if (rem <= 0) break;
        fd_set rf; FD_ZERO(&rf); FD_SET(ser, &rf);
        struct timeval tv = { rem / 1000, (rem % 1000) * 1000 };
        if (select(ser + 1, &rf, NULL, NULL, &tv) <= 0) break;
        int n = read(ser, buf + total, sizeof(buf) - total);
        if (n <= 0) break;
        total += n;
    }
#endif

    if (total == 0)
        return NULL;

    /* NMEA */
    if (total >= 3 && buf[0] == '$' &&
        (memcmp(buf+1,"GP",2)==0 || memcmp(buf+1,"GN",2)==0 ||
         memcmp(buf+1,"GL",2)==0 || memcmp(buf+1,"BD",2)==0))
        return "GPS/NMEA stream";

    /* AT modem */
    if (total >= 2 && buf[0] == 'A' && buf[1] == 'T') return "AT/LTE modem";
    if (total >= 2 && buf[0] == '+' && buf[1] == 'C') return "AT/LTE modem";
    for (int i = 0; i + 2 < total; i++)
        if (buf[i]=='O' && buf[i+1]=='K' &&
            (buf[i+2]=='\r' || buf[i+2]=='\n')) return "AT/LTE modem";

    /* ESP boot */
    for (int i = 0; i + 4 <= total; i++)
        if (memcmp(buf+i,"rst:",4)==0 || memcmp(buf+i,"ets ",4)==0 ||
            memcmp(buf+i,"load",4)==0) return "ESP8266/ESP32 boot log";

    /* Modbus master */
    if (total >= 5) {
        int exp = (buf[1] & 0x80) ? 5 : (3 + buf[2] + 2);
        if (exp >= 5 && exp <= total) {
            uint16_t cc = modbus_crc16(buf, exp - 2);
            uint16_t cr = buf[exp-2] | (buf[exp-1] << 8);
            if (cc == cr) return "Modbus master traffic";
        }
    }

    /* printable vs binary */
    int pr = 0;
    for (int i = 0; i < total; i++)
        if ((buf[i] >= 0x20 && buf[i] < 0x7F) ||
             buf[i]=='\r' || buf[i]=='\n' || buf[i]=='\t') pr++;

    return (pr * 100 / total >= 70) ? "unknown text protocol" : "unknown binary data";
}


/* ================================================================
 * Scan engine
 * ================================================================ */

/*
 * Scan a range of addresses with given serial parameters.
 * Returns number of devices found.
 * If stop_first is set, returns after first found device.
 */
static int scan_range(serial_t ser, int from, int to, int timeout,
                      int start_reg, int count, int baud,
                      char parity, int databits, int stopbits,
                      int verbose, int stop_first,
                      const char *combo_prefix, const char *port_name)
{
    int total_addrs = to - from + 1;
    int found = 0;
    int write_errors = 0;

    for (int addr = from; addr <= to; addr++) {
        int progress = addr - from + 1;

        if (verbose) {
            fprintf(stderr, "%s[%d/%d] Trying address %d...\n",
                    combo_prefix, progress, total_addrs, addr);
        } else {
            fprintf(stderr, "\r%s[%d/%d] Scanning address %d...    ",
                    combo_prefix, progress, total_addrs, addr);
        }

        flush_serial(ser);

        uint8_t req[8];
        build_fc03_request(req, (uint8_t)addr, (uint16_t)start_reg, (uint16_t)count);

        int written = write_serial(ser, req, 8);
        if (written != 8) {
#ifdef _WIN32
            fprintf(stderr, "\nWrite error at address %d: error %lu\n", addr, GetLastError());
#else
            fprintf(stderr, "\nWrite error at address %d: %s\n", addr, strerror(errno));
#endif
            write_errors++;
            if (write_errors >= 3) {
                fprintf(stderr,
                    "mbscan: 3 consecutive write errors on %s at %d-%c%c%c\n"
                    "mbscan: port may be occupied by another device.\n"
                    "mbscan: listening for incoming data (500ms)...\n",
                    port_name, baud, '0' + databits, parity, '0' + stopbits);

                const char *what = passive_sniff(ser, 500);
                if (what)
                    fprintf(stderr, "mbscan: detected: %s\n", what);
                else
                    fprintf(stderr, "mbscan: no incoming data (wrong baud rate or hardware issue).\n");

                fprintf(stderr, "mbscan: skipping port %s.\n", port_name);
                return -1;
            }
            continue;
        }
        write_errors = 0;

        uint8_t resp[256];
        int rlen = read_response(ser, resp, sizeof(resp), timeout);

        if (rlen > 0 && validate_response(resp, rlen, (uint8_t)addr)) {
            found++;
            int byte_count = resp[2];
            int nregs = byte_count / 2;

            /* Store result */
            if (found_count < MAX_FOUND) {
                found_device_t *d = &found_devices[found_count++];
                d->port = port_name;
                d->addr = addr;
                d->baud = baud;
                d->databits = databits;
                d->parity = parity;
                d->stopbits = stopbits;
                d->start_reg = start_reg;
                d->nregs = (nregs < count) ? nregs : count;
                for (int i = 0; i < d->nregs; i++)
                    d->regs[i] = (resp[3 + i * 2] << 8) | resp[3 + i * 2 + 1];
            }

            fprintf(stderr, "\n");
            printf("Found slave %3d at %d-%c%c%c:", addr, baud,
                   '0' + databits, parity, '0' + stopbits);

            for (int i = 0; i < nregs && i < count; i++) {
                uint16_t val = (resp[3 + i * 2] << 8) | resp[3 + i * 2 + 1];
                printf(" [%d]=%u", start_reg + i, val);
            }
            printf("\n");
            fflush(stdout);

            if (stop_first)
                return found;
        } else if (verbose && rlen > 0) {
            fprintf(stderr, " -- got %d bytes but invalid response\n", rlen);
        }

        inter_frame_delay(baud);
    }

    return found;
}


/* ================================================================
 * Universal scan mode tables
 * ================================================================ */

/* Baud rates for -B and -T */
static const int u_bauds[] = { 9600, 19200, 38400, 115200 };
#define U_NBAUDS (int)(sizeof(u_bauds) / sizeof(u_bauds[0]))

/* Data formats for -D and -T */
typedef struct {
    int databits;
    char parity;
    int stopbits;
    const char *label; /* e.g. "8N1" */
} dataformat_t;

static const dataformat_t u_formats[] = {
    { 8, 'N', 1, "8N1" },
    { 8, 'E', 1, "8E1" },
};
#define U_NFORMATS (int)(sizeof(u_formats) / sizeof(u_formats[0]))


/* ================================================================
 * Usage and main
 * ================================================================ */

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options] PORT\n"
        "       %s -p PORT [options]\n"
        "\n"
        "Modbus RTU bus scanner. Sends FC03 to each address and reports responses.\n"
        "\n"
        "Options:\n"
        "  -p PORT    Serial port (or pass PORT as last argument)\n"
#ifdef _WIN32
        "             Windows: COM3, COM10, \\\\.\\COM15\n"
#else
        "             Linux: /dev/ttyUSB0, /dev/ttyS1\n"
#endif
        "  -b BAUD    Baud rate (default: 115200)\n"
        "  -d PARAMS  Data format: 8N1, 8E1, 8O1, 7E1, etc. (default: 8N1)\n"
        "  -f FROM    Start address (default: 1)\n"
        "  -t TO      End address (default: 247)\n"
        "  -o MS      Timeout per address in ms (default: 100)\n"
        "  -r REG     Start register, 0-based (default: 0)\n"
        "  -c COUNT   Number of registers to read (default: 1)\n"
        "  -T         Total scan: try all baud rates x data formats\n"
        "               Bauds: 9600, 19200, 38400, 115200\n"
        "               Formats: 8N1, 8E1\n"
        "  -B         Scan baud rates only (9600..115200), keep format from -d\n"
        "  -D         Scan data formats only (8N1, 8E1), keep baud from -b\n"
        "  -S         Stop after first device found\n"
        "  -R         Print summary report at the end\n"
        "  -v         Verbose output\n"
        "  -h         Show this help\n"
        "\n"
        "Examples:\n"
#ifdef _WIN32
        "  %s COM3\n"
        "  %s -T -S COM3\n"
        "  %s -b 9600 -d 8E1 -f 1 -t 30 -o 200 COM5\n"
#else
        "  %s /dev/ttyUSB0\n"
        "  %s -T -S /dev/ttyUSB0\n"
        "  %s -b 9600 -d 8E1 -f 1 -t 30 -o 200 /dev/ttyS1\n"
#endif
        "\n",
        prog, prog, prog, prog, prog);
}

int main(int argc, char *argv[])
{
    const char *port_arg = NULL;  /* raw -p value */
    int baud      = 115200;
    char parity   = 'N';
    int databits  = 8;
    int stopbits  = 1;
    int from      = 1;
    int to        = 247;
    int timeout   = 100;
    int start_reg = 0;
    int count     = 1;
    int verbose   = 0;
    int umode     = 0;   /* 0=normal, 1=bauds, 2=formats, 3=total */
    int stop_first = 0;
    int report     = 0;

    int opt;
    while ((opt = getopt(argc, argv, "p:b:d:f:t:o:r:c:TBDSRvh")) != -1) {
        switch (opt) {
        case 'p': port_arg = optarg; break;
        case 'b': baud = atoi(optarg); break;
        case 'd':
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
        case 'T': umode = 3; break;
        case 'B': umode = 1; break;
        case 'D': umode = 2; break;
        case 'S': stop_first = 1; break;
        case 'R': report = 1; break;
        case 'v': verbose = 1; break;
        case 'h': usage(argv[0]); return 0;
        default:  usage(argv[0]); return 1;
        }
    }

    /* Build port list: from -p (comma-separated) + positional args */
#define MAX_PORTS 32
    const char *ports[MAX_PORTS];
    int nports = 0;

    /* Split -p value by comma */
    static char port_buf[1024];
    if (port_arg) {
        strncpy(port_buf, port_arg, sizeof(port_buf) - 1);
        char *tok = strtok(port_buf, ",");
        while (tok && nports < MAX_PORTS) {
            ports[nports++] = tok;
            tok = strtok(NULL, ",");
        }
    }

    /* Positional arguments */
    while (optind < argc && nports < MAX_PORTS)
        ports[nports++] = argv[optind++];

    if (nports == 0) {
        fprintf(stderr, "Error: PORT is required\n\n");
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

    /* Build combo list */
    typedef struct { int baud; int databits; char parity; int stopbits; } combo_t;
    combo_t combos[32];
    int ncombos = 0;

    switch (umode) {
    case 0:
        combos[0].baud     = baud;
        combos[0].databits = databits;
        combos[0].parity   = parity;
        combos[0].stopbits = stopbits;
        ncombos = 1;
        break;
    case 1:
        for (int i = 0; i < U_NBAUDS; i++) {
            combos[ncombos].baud     = u_bauds[i];
            combos[ncombos].databits = databits;
            combos[ncombos].parity   = parity;
            combos[ncombos].stopbits = stopbits;
            ncombos++;
        }
        break;
    case 2:
        for (int i = 0; i < U_NFORMATS; i++) {
            combos[ncombos].baud     = baud;
            combos[ncombos].databits = u_formats[i].databits;
            combos[ncombos].parity   = u_formats[i].parity;
            combos[ncombos].stopbits = u_formats[i].stopbits;
            ncombos++;
        }
        break;
    case 3:
        for (int i = 0; i < U_NBAUDS; i++) {
            for (int j = 0; j < U_NFORMATS; j++) {
                combos[ncombos].baud     = u_bauds[i];
                combos[ncombos].databits = u_formats[j].databits;
                combos[ncombos].parity   = u_formats[j].parity;
                combos[ncombos].stopbits = u_formats[j].stopbits;
                ncombos++;
            }
        }
        break;
    }

    fprintf(stderr,
        "mbscan: scanning for Modbus RTU slaves (addresses %d-%d).\n"
        "mbscan: use only on ports where Modbus sensors are expected.\n\n",
        from, to);

    uint64_t scan_start = now_ms();
    int total_found = 0;
    int stopped_early = 0;

    /* Loop over all ports */
    for (int pi = 0; pi < nports; pi++) {
        const char *port = ports[pi];

        if (nports > 1)
            fprintf(stderr, "\n=== Port %s [%d/%d] ===\n", port, pi + 1, nports);

        serial_t ser = open_serial(port, combos[0].baud, combos[0].parity,
                                   combos[0].databits, combos[0].stopbits);
        if (ser == SERIAL_INVALID) {
            fprintf(stderr, "mbscan: skipping %s (cannot open)\n", port);
            continue;
        }

        int total_addrs = to - from + 1;
        int total_probes = ncombos * total_addrs;
        int est_secs = (total_probes * timeout) / 1000;

        if (ncombos > 1) {
            char eta_buf[16];
            fmt_time(est_secs, eta_buf);
            fprintf(stderr, "mbscan: universal scan on %s, addresses %d-%d, timeout %dms\n",
                    port, from, to, timeout);
            fprintf(stderr, "mbscan: scanning %d combination(s), ETA ~%s\n",
                    ncombos, eta_buf);
            fprintf(stderr, "mbscan: reading %d register(s) starting at %d\n\n",
                    count, start_reg);
        } else {
            fprintf(stderr, "mbscan: scanning %s %d-%c%c%c, addresses %d-%d, timeout %dms\n",
                    port, combos[0].baud, '0' + combos[0].databits,
                    combos[0].parity, '0' + combos[0].stopbits, from, to, timeout);
            fprintf(stderr, "mbscan: reading %d register(s) starting at %d\n\n",
                    count, start_reg);
        }

        uint64_t port_start = now_ms();

        for (int ci = 0; ci < ncombos; ci++) {
            combo_t *c = &combos[ci];

            if (ci > 0) {
                if (reconfig_serial(ser, c->baud, c->parity, c->databits, c->stopbits) < 0) {
                    fprintf(stderr, "Failed to reconfigure port to %d-%c%c%c\n",
                            c->baud, '0' + c->databits, c->parity, '0' + c->stopbits);
                    continue;
                }
                sleep_ms(50);
            }

            if (ncombos > 1) {
                uint64_t elapsed_ms = now_ms() - port_start;
                int elapsed_s = (int)(elapsed_ms / 1000);
                int remaining_s = (ci > 0)
                    ? (int)((double)elapsed_s * ncombos / ci) - elapsed_s
                    : est_secs;

                char el_buf[16], rem_buf[16];
                fmt_time(elapsed_s, el_buf);
                fmt_time(remaining_s, rem_buf);

                fprintf(stderr, "\n[%d/%d] %d-%c%c%c  (%s elapsed, ~%s remaining)\n",
                        ci + 1, ncombos, c->baud,
                        '0' + c->databits, c->parity, '0' + c->stopbits,
                        el_buf, rem_buf);
            }

            char prefix[64] = "";
            if (ncombos > 1) {
                snprintf(prefix, sizeof(prefix), "[%d/%d %d-%c%c%c] ",
                         ci + 1, ncombos, c->baud,
                         '0' + c->databits, c->parity, '0' + c->stopbits);
            }

            int n = scan_range(ser, from, to, timeout, start_reg, count,
                               c->baud, c->parity, c->databits, c->stopbits,
                               verbose, stop_first, prefix, port);

            if (n == -1)
                break;  /* port fatal error — skip remaining combos */

            total_found += n;

            if (stop_first && n > 0) {
                stopped_early = 1;
                break;
            }
        }

        close_serial(ser);

        if (stopped_early)
            break;
    }

    /* Final summary */
    uint64_t total_ms = now_ms() - scan_start;
    char total_buf[16];
    fmt_time((int)(total_ms / 1000), total_buf);

    if ((report || ncombos > 1 || nports > 1) && found_count > 0) {
        fprintf(stderr, "\n\n=== Scan results ===\n");
        for (int i = 0; i < found_count; i++) {
            found_device_t *d = &found_devices[i];
            fprintf(stderr, "  %-16s  Slave %3d at %d-%c%c%c:",
                    d->port, d->addr, d->baud,
                    '0' + d->databits, d->parity, '0' + d->stopbits);
            for (int j = 0; j < d->nregs; j++)
                fprintf(stderr, " [%d]=%u", d->start_reg + j, d->regs[j]);
            fprintf(stderr, "\n");
        }
    }

    if (stopped_early)
        fprintf(stderr, "\n\nmbscan: stopped (-S). Found %d device(s) in %s.\n",
                total_found, total_buf);
    else
        fprintf(stderr, "\n\nmbscan: done in %s. Found %d device(s).\n",
                total_buf, total_found);

    return total_found > 0 ? 0 : 1;
}
