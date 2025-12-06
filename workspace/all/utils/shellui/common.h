#ifndef SHELLUI_COMMON_H
#define SHELLUI_COMMON_H

#include <stdbool.h>

// Exit codes matching existing minui-* utilities
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

// Commands supported by shellui
typedef enum {
	CMD_NONE = 0,
	CMD_MESSAGE,
	CMD_LIST,
	CMD_KEYBOARD,
	CMD_START,
	CMD_SHUTDOWN,
} CommandType;

// IPC paths
#define SHELLUI_DIR "/tmp/shellui"
#define SHELLUI_PID_FILE SHELLUI_DIR "/pid"
#define SHELLUI_READY_FILE SHELLUI_DIR "/ready"
#define SHELLUI_REQUEST_FILE SHELLUI_DIR "/request"
#define SHELLUI_RESPONSE_FILE SHELLUI_DIR "/response"

// Timeouts
#define DAEMON_STARTUP_TIMEOUT_MS 5000
#define RESPONSE_POLL_INTERVAL_MS 10
#define RESPONSE_TIMEOUT_MS 300000  // 5 minutes max for user interaction

#endif // SHELLUI_COMMON_H
