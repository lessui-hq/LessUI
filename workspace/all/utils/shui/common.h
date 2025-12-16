#ifndef SHUI_COMMON_H
#define SHUI_COMMON_H

#include <stdbool.h>

// Exit codes matching existing launcher-* utilities
typedef enum {
	EXIT_SUCCESS_CODE = 0,
	EXIT_ERROR = 1,
	EXIT_CANCEL = 2,
	EXIT_MENU = 3,
	EXIT_ACTION = 4,
	EXIT_INACTION = 5,
	EXIT_START = 6,
	EXIT_PARSE_ERROR = 10,
	EXIT_SERIALIZE_ERROR = 11,
	EXIT_TIMEOUT = 124,
	EXIT_SIGINT = 130,
	EXIT_SIGTERM = 143,
} ExitCode;

// Commands supported by shui
typedef enum {
	CMD_NONE = 0,
	CMD_MESSAGE,
	CMD_LIST,
	CMD_KEYBOARD,
	CMD_PROGRESS,
	CMD_START,
	CMD_SHUTDOWN,
	CMD_AUTO_SLEEP,
	CMD_RESTART,
} CommandType;

// IPC paths
#define SHUI_DIR "/tmp/shui"
#define SHUI_PID_FILE SHUI_DIR "/pid"
#define SHUI_READY_FILE SHUI_DIR "/ready"
#define SHUI_REQUEST_FILE SHUI_DIR "/request"
#define SHUI_RESPONSE_FILE SHUI_DIR "/response"
#define SHUI_LOCK_FILE SHUI_DIR "/spawn.lock"

// Timeouts
#define DAEMON_STARTUP_TIMEOUT_MS 5000
#define RESPONSE_POLL_INTERVAL_MS 10
#define RESPONSE_TIMEOUT_MS 300000  // 5 minutes max for user interaction

#endif // SHUI_COMMON_H
