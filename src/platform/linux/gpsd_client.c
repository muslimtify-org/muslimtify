#define _POSIX_C_SOURCE 200809L

#include "gpsd_client.h"

#include "json.h"
#include "platform_native.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

// Max bytes of a single gpsd JSON line the parser will consider. Longer lines
// are ignored (never grown or partially parsed).
#define GPSD_LINE_MAX 8192

void gpsd_scan_line(const char *line, GpsdScan *scan) {
  if (!line || !scan)
    return;

  // json get_value wants a mutable, NUL-terminated buffer; copy into a bounded
  // stack buffer and drop anything at or over the cap.
  char buf[GPSD_LINE_MAX];
  size_t n = strlen(line);
  if (n == 0 || n >= sizeof(buf))
    return;
  memcpy(buf, line, n + 1);

  JsonContext *ctx = json_begin();
  if (!ctx)
    return;

  char *cls = get_value(ctx, "class", buf);
  if (cls) {
    // Any per-device report means gpsd has a device, even without a fix.
    if (strcmp(cls, "TPV") == 0 || strcmp(cls, "SKY") == 0 || strcmp(cls, "DEVICE") == 0)
      scan->saw_device = true;

    if (strcmp(cls, "TPV") == 0) {
      char *mode = get_value(ctx, "mode", buf);
      char *lat = get_value(ctx, "lat", buf);
      char *lon = get_value(ctx, "lon", buf);
      if (mode && lat && lon && atoi(mode) >= 2) {
        char *elat = NULL;
        char *elon = NULL;
        double dlat = strtod(lat, &elat);
        double dlon = strtod(lon, &elon);
        // Require a full numeric parse and a valid geographic range.
        if (elat != lat && elon != lon && isfinite(dlat) && isfinite(dlon) &&
            dlat >= -90.0 && dlat <= 90.0 && dlon >= -180.0 && dlon <= 180.0) {
          scan->lat = dlat;
          scan->lng = dlon;
          scan->have_fix = true;
        }
      }
    }
  }

  json_end(ctx);
}

#define GPSD_PORT 2947
#define GPSD_CONNECT_TIMEOUT_MS 2000
#define GPSD_READ_DEADLINE_MS 5000
#define GPSD_MAX_LINES 256
#define GPSD_WATCH "?WATCH={\"enable\":true,\"json\":true};\n"

// Monotonic milliseconds, for a wall-clock read deadline independent of any
// single recv timeout.
static long gpsd_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Connect to 127.0.0.1:2947 with a bounded, non-blocking connect. Loopback
// only: no hostname, no DNS. Returns a blocking fd (>=0) with recv/send
// timeouts set, or -1 on any failure.
static int gpsd_connect(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  int flags = fcntl(fd, F_GETFL, 0);
  if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    close(fd);
    return -1;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(GPSD_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (rc < 0 && errno == EINPROGRESS) {
    struct pollfd pfd = {.fd = fd, .events = POLLOUT};
    int pr;
    do {
      pr = poll(&pfd, 1, GPSD_CONNECT_TIMEOUT_MS);
    } while (pr < 0 && errno == EINTR);
    if (pr <= 0) {
      close(fd);
      return -1;
    }
    int soerr = 0;
    socklen_t len = sizeof(soerr);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &soerr, &len) < 0 || soerr != 0) {
      close(fd);
      return -1;
    }
  } else if (rc < 0) {
    close(fd);
    return -1;
  }

  // Back to blocking, with a receive/send timeout as a second read bound.
  if (fcntl(fd, F_SETFL, flags) < 0) {
    close(fd);
    return -1;
  }
  struct timeval tv = {.tv_sec = GPSD_READ_DEADLINE_MS / 1000, .tv_usec = 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  return fd;
}

// Send all bytes, tolerating short writes and EINTR. Returns 0 on success.
static int gpsd_send_all(int fd, const char *data, size_t len) {
  size_t off = 0;
  while (off < len) {
    ssize_t w = send(fd, data + off, len - off, MSG_NOSIGNAL);
    if (w < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    off += (size_t)w;
  }
  return 0;
}

GpsStatus platform_native_get_location(PlatformLatLng *latlong) {
  if (!latlong)
    return GPS_NO_FIX;

  int fd = gpsd_connect();
  if (fd < 0)
    return GPS_NO_DAEMON;

  if (gpsd_send_all(fd, GPSD_WATCH, strlen(GPSD_WATCH)) != 0) {
    close(fd);
    return GPS_NO_DAEMON;
  }

  GpsdScan scan = {0};
  char line[GPSD_LINE_MAX];
  size_t line_len = 0;
  bool line_overflow = false;
  int lines_seen = 0;
  char rbuf[2048];
  long deadline = gpsd_now_ms() + GPSD_READ_DEADLINE_MS;

  while (!scan.have_fix && lines_seen < GPSD_MAX_LINES) {
    if (gpsd_now_ms() >= deadline)
      break;

    ssize_t r = recv(fd, rbuf, sizeof(rbuf), 0);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      break; // timeout (SO_RCVTIMEO) or error
    }
    if (r == 0)
      break; // peer closed

    for (ssize_t i = 0; i < r && !scan.have_fix; i++) {
      char c = rbuf[i];
      if (c == '\n') {
        if (!line_overflow) {
          line[line_len] = '\0';
          gpsd_scan_line(line, &scan);
          lines_seen++;
        }
        line_len = 0;
        line_overflow = false;
        if (lines_seen >= GPSD_MAX_LINES)
          break;
      } else if (!line_overflow) {
        if (line_len < sizeof(line) - 1) {
          line[line_len++] = c;
        } else {
          line_overflow = true; // drop the remainder of an over-long line
        }
      }
    }
  }

  close(fd);

  if (scan.have_fix) {
    *latlong = (PlatformLatLng){.lat = scan.lat, .lng = scan.lng};
    return GPS_OK;
  }
  if (!scan.saw_device)
    return GPS_NO_DEVICE;
  return GPS_NO_FIX;
}
