#ifndef SHELLUI_IPC_H
#define SHELLUI_IPC_H

#include "common.h"
#include <parson/parson.h>

// Request structure sent from CLI to daemon
typedef struct {
	CommandType command;
	char* request_id;

	// === Common params ===
	char* background_color;
	char* background_image;
	char* confirm_button;      // Physical button (default: "A")
	char* confirm_text;        // Button label
	char* cancel_button;       // Physical button (default: "B")
	char* cancel_text;         // Button label
	char* action_button;       // Physical button for action
	char* action_text;         // Action button label
	bool disable_auto_sleep;
	bool show_hardware_group;

	// === Message command params ===
	char* message;
	int timeout;               // -1 = forever, 0+ = seconds
	bool show_pill;
	bool show_time_left;
	char* message_alignment;   // "left", "center", "right"
	bool confirm_show;
	bool cancel_show;
	bool action_show;
	char* inaction_button;
	char* inaction_text;
	bool inaction_show;
	bool quit_after_last_item;

	// === List command params ===
	char* file_path;
	char* format;              // "json" or "text"
	char* title;
	char* title_alignment;     // "left", "center", "right"
	char* item_key;
	char* stdin_data;          // for piped input
	char* write_location;      // file path or "-" for stdout
	char* write_value;         // "selected", "state", "name", "value"
	char* enable_button;       // Physical button for enable toggle

	// === Keyboard command params ===
	char* initial_value;
	// write_location is shared with list
} Request;

// Response structure sent from daemon to CLI
typedef struct {
	char* request_id;
	ExitCode exit_code;
	char* output;              // selected value, entered text, or JSON state
	int selected_index;        // index of selected item
} Response;

// Initialize IPC (create directory, clean stale files)
int ipc_init(void);

// Cleanup IPC directory
void ipc_cleanup(void);

// Write a request to the request file
int ipc_write_request(const Request* req);

// Read a request from the request file (daemon side)
// Returns NULL if no request or parse error
Request* ipc_read_request(void);

// Free request fields (for stack-allocated Request)
void ipc_free_request_fields(Request* req);

// Free request structure (for heap-allocated Request from ipc_read_request)
void ipc_free_request(Request* req);

// Write a response to the response file
int ipc_write_response(const Response* resp);

// Read a response from the response file (CLI side)
// Returns NULL if no response or parse error
Response* ipc_read_response(void);

// Free a response structure
void ipc_free_response(Response* resp);

// Wait for response file to appear (with timeout)
// Returns 0 on success, -1 on timeout
int ipc_wait_for_response(int timeout_ms);

// Delete request file (daemon does this after reading)
void ipc_delete_request(void);

// Delete response file (CLI does this after reading)
void ipc_delete_response(void);

// Generate unique request ID
char* ipc_generate_request_id(void);

#endif // SHELLUI_IPC_H
