#include "ipc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

// Helper to duplicate string (or return NULL)
static char* safe_strdup(const char* s) {
	return s ? strdup(s) : NULL;
}

// Get JSON string or NULL
static const char* json_get_string(JSON_Object* obj, const char* name) {
	return json_object_get_string(obj, name);
}

// Get JSON int with default
static int json_get_int(JSON_Object* obj, const char* name, int def) {
	JSON_Value* val = json_object_get_value(obj, name);
	if (val && json_value_get_type(val) == JSONNumber) {
		return (int)json_object_get_number(obj, name);
	}
	return def;
}

// Get JSON bool with default
static bool json_get_bool(JSON_Object* obj, const char* name, bool def) {
	JSON_Value* val = json_object_get_value(obj, name);
	if (val && json_value_get_type(val) == JSONBoolean) {
		return json_object_get_boolean(obj, name) != 0;
	}
	return def;
}

int ipc_init(void) {
	// Create directory if needed
	if (mkdir(SHELLUI_DIR, 0755) != 0 && errno != EEXIST) {
		return -1;
	}

	// Clean stale files
	unlink(SHELLUI_REQUEST_FILE);
	unlink(SHELLUI_RESPONSE_FILE);

	return 0;
}

void ipc_cleanup(void) {
	unlink(SHELLUI_REQUEST_FILE);
	unlink(SHELLUI_RESPONSE_FILE);
	unlink(SHELLUI_READY_FILE);
	unlink(SHELLUI_PID_FILE);
	rmdir(SHELLUI_DIR);
}

int ipc_write_request(const Request* req) {
	JSON_Value* root = json_value_init_object();
	JSON_Object* obj = json_object(root);

	// Command type
	const char* cmd_str = "none";
	switch (req->command) {
		case CMD_MESSAGE:
			cmd_str = "message";
			break;
		case CMD_LIST:
			cmd_str = "list";
			break;
		case CMD_KEYBOARD:
			cmd_str = "keyboard";
			break;
		case CMD_SHUTDOWN:
			cmd_str = "shutdown";
			break;
		default:
			break;
	}
	json_object_set_string(obj, "command", cmd_str);

	if (req->request_id) {
		json_object_set_string(obj, "request_id", req->request_id);
	}

	// Message params
	if (req->message) {
		json_object_set_string(obj, "message", req->message);
	}
	json_object_set_number(obj, "timeout", req->timeout);
	if (req->background_color) {
		json_object_set_string(obj, "background_color", req->background_color);
	}
	if (req->background_image) {
		json_object_set_string(obj, "background_image", req->background_image);
	}
	if (req->confirm_text) {
		json_object_set_string(obj, "confirm_text", req->confirm_text);
	}
	if (req->cancel_text) {
		json_object_set_string(obj, "cancel_text", req->cancel_text);
	}
	json_object_set_boolean(obj, "show_pill", req->show_pill);

	// List params
	if (req->file_path) {
		json_object_set_string(obj, "file_path", req->file_path);
	}
	if (req->format) {
		json_object_set_string(obj, "format", req->format);
	}
	if (req->title) {
		json_object_set_string(obj, "title", req->title);
	}
	if (req->item_key) {
		json_object_set_string(obj, "item_key", req->item_key);
	}
	if (req->stdin_data) {
		json_object_set_string(obj, "stdin_data", req->stdin_data);
	}

	// Keyboard params
	if (req->initial_value) {
		json_object_set_string(obj, "initial_value", req->initial_value);
	}

	// Write to file
	char* json_str = json_serialize_to_string_pretty(root);
	if (!json_str) {
		json_value_free(root);
		return -1;
	}

	FILE* f = fopen(SHELLUI_REQUEST_FILE, "w");
	if (!f) {
		json_free_serialized_string(json_str);
		json_value_free(root);
		return -1;
	}

	fputs(json_str, f);
	fclose(f);

	json_free_serialized_string(json_str);
	json_value_free(root);
	return 0;
}

Request* ipc_read_request(void) {
	JSON_Value* root = json_parse_file(SHELLUI_REQUEST_FILE);
	if (!root) {
		return NULL;
	}

	JSON_Object* obj = json_object(root);
	if (!obj) {
		json_value_free(root);
		return NULL;
	}

	Request* req = calloc(1, sizeof(Request));
	if (!req) {
		json_value_free(root);
		return NULL;
	}

	// Parse command
	const char* cmd_str = json_get_string(obj, "command");
	if (cmd_str) {
		if (strcmp(cmd_str, "message") == 0) {
			req->command = CMD_MESSAGE;
		} else if (strcmp(cmd_str, "list") == 0) {
			req->command = CMD_LIST;
		} else if (strcmp(cmd_str, "keyboard") == 0) {
			req->command = CMD_KEYBOARD;
		} else if (strcmp(cmd_str, "shutdown") == 0) {
			req->command = CMD_SHUTDOWN;
		}
	}

	req->request_id = safe_strdup(json_get_string(obj, "request_id"));

	// Message params
	req->message = safe_strdup(json_get_string(obj, "message"));
	req->timeout = json_get_int(obj, "timeout", -1);
	req->background_color = safe_strdup(json_get_string(obj, "background_color"));
	req->background_image = safe_strdup(json_get_string(obj, "background_image"));
	req->confirm_text = safe_strdup(json_get_string(obj, "confirm_text"));
	req->cancel_text = safe_strdup(json_get_string(obj, "cancel_text"));
	req->show_pill = json_get_bool(obj, "show_pill", false);

	// List params
	req->file_path = safe_strdup(json_get_string(obj, "file_path"));
	req->format = safe_strdup(json_get_string(obj, "format"));
	req->title = safe_strdup(json_get_string(obj, "title"));
	req->item_key = safe_strdup(json_get_string(obj, "item_key"));
	req->stdin_data = safe_strdup(json_get_string(obj, "stdin_data"));

	// Keyboard params
	req->initial_value = safe_strdup(json_get_string(obj, "initial_value"));

	json_value_free(root);
	return req;
}

void ipc_free_request(Request* req) {
	if (!req) return;
	free(req->request_id);
	free(req->message);
	free(req->background_color);
	free(req->background_image);
	free(req->confirm_text);
	free(req->cancel_text);
	free(req->file_path);
	free(req->format);
	free(req->title);
	free(req->item_key);
	free(req->stdin_data);
	free(req->initial_value);
	free(req);
}

int ipc_write_response(const Response* resp) {
	JSON_Value* root = json_value_init_object();
	JSON_Object* obj = json_object(root);

	if (resp->request_id) {
		json_object_set_string(obj, "request_id", resp->request_id);
	}
	json_object_set_number(obj, "exit_code", resp->exit_code);
	if (resp->output) {
		json_object_set_string(obj, "output", resp->output);
	}

	char* json_str = json_serialize_to_string_pretty(root);
	if (!json_str) {
		json_value_free(root);
		return -1;
	}

	FILE* f = fopen(SHELLUI_RESPONSE_FILE, "w");
	if (!f) {
		json_free_serialized_string(json_str);
		json_value_free(root);
		return -1;
	}

	fputs(json_str, f);
	fclose(f);

	json_free_serialized_string(json_str);
	json_value_free(root);
	return 0;
}

Response* ipc_read_response(void) {
	JSON_Value* root = json_parse_file(SHELLUI_RESPONSE_FILE);
	if (!root) {
		return NULL;
	}

	JSON_Object* obj = json_object(root);
	if (!obj) {
		json_value_free(root);
		return NULL;
	}

	Response* resp = calloc(1, sizeof(Response));
	if (!resp) {
		json_value_free(root);
		return NULL;
	}

	resp->request_id = safe_strdup(json_get_string(obj, "request_id"));
	resp->exit_code = json_get_int(obj, "exit_code", EXIT_ERROR);
	resp->output = safe_strdup(json_get_string(obj, "output"));

	json_value_free(root);
	return resp;
}

void ipc_free_response(Response* resp) {
	if (!resp) return;
	free(resp->request_id);
	free(resp->output);
	free(resp);
}

int ipc_wait_for_response(int timeout_ms) {
	struct timeval start, now;
	gettimeofday(&start, NULL);

	while (1) {
		// Check if response file exists
		if (access(SHELLUI_RESPONSE_FILE, F_OK) == 0) {
			return 0;
		}

		// Check timeout
		gettimeofday(&now, NULL);
		long elapsed = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000;
		if (elapsed >= timeout_ms) {
			return -1;
		}

		// Sleep before next poll
		usleep(RESPONSE_POLL_INTERVAL_MS * 1000);
	}
}

void ipc_delete_request(void) {
	unlink(SHELLUI_REQUEST_FILE);
}

void ipc_delete_response(void) {
	unlink(SHELLUI_RESPONSE_FILE);
}

char* ipc_generate_request_id(void) {
	struct timeval tv;
	gettimeofday(&tv, NULL);

	char* id = malloc(32);
	if (id) {
		snprintf(id, 32, "%ld%06ld", tv.tv_sec, tv.tv_usec);
	}
	return id;
}
