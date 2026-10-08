#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Where core sends its diagnostics. Core never writes to a terminal itself: it
   hands each message to one handler, which a frontend can replace. */

typedef enum {
  MT_LOG_WARNING, /* something was off, and the operation carried on */
  MT_LOG_ERROR    /* the operation that was asked for failed */
} MtLogLevel;

typedef void (*MtLogHandler)(MtLogLevel level, const char *message, void *user_data);

/* Longest message, including its terminator. A longer one is truncated. */
enum { MT_LOG_MESSAGE_MAX = 1024 };

/**
 * Pass one finished message, with no trailing newline, to the current handler.
 * Does nothing for a NULL message or when the handler is NULL.
 */
void mt_log(MtLogLevel level, const char *message);

/** Replace the handler. NULL discards every message. */
void mt_log_set_handler(MtLogHandler handler, void *user_data);

/** The handler in place at startup: the message and a newline to stderr. */
void mt_log_stderr(MtLogLevel level, const char *message, void *user_data);

/**
 * Format a message as printf would, then log it. A macro over snprintf, so the
 * compiler checks the format against its arguments at every call site. The
 * length is stored only so that the truncation diagnostic, which fires when a
 * bounded print's result is ignored, stays quiet: truncating is intended here.
 */
#define MT_LOGF(level, ...)                                                                \
  do {                                                                                     \
    char mt_logf_buffer_[MT_LOG_MESSAGE_MAX];                                              \
    int mt_logf_length_ = snprintf(mt_logf_buffer_, sizeof(mt_logf_buffer_), __VA_ARGS__); \
    (void)mt_logf_length_;                                                                 \
    mt_log((level), mt_logf_buffer_);                                                      \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif // LOG_H
