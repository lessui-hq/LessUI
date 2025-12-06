#include "ipc.h"
#include "utils.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

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

// Helper to set string field if not NULL
static void json_set_string_if(JSON_Object* obj, const char* name, const char* value) {
	if (value) json_object_set_string(obj, name, value);
}

int ipc_write_request(const Request* req) {
	JSON_Value* root = json_value_init_object();
	JSON_Object* obj = json_object(root);

	// Command type
	const char* cmd_str = "none";
	switch (req->command) {
		case CMD_MESSAGE: cmd_str = "message"; break;
		case CMD_LIST: cmd_str = "list"; break;
		case CMD_KEYBOARD: cmd_str = "keyboard"; break;
		case CMD_PROGRESS: cmd_str = "progress"; break;
		case CMD_SHUTDOWN: cmd_str = "shutdown"; break;
		default: break;
	}
	json_object_set_string(obj, "command", cmd_str);
	json_set_string_if(obj, "request_id", req->request_id);

	// Message params
	json_set_string_if(obj, "message", req->message);
	json_object_set_number(obj, "timeout", req->timeout);
	json_set_string_if(obj, "background_color", req->background_color);
	json_set_string_if(obj, "background_image", req->background_image);
	json_set_string_if(obj, "confirm_text", req->confirm_text);
	json_set_string_if(obj, "cancel_text", req->cancel_text);
	json_object_set_boolean(obj, "show_pill", req->show_pill);
	json_object_set_boolean(obj, "show_time_left", req->show_time_left);

	// List params
	json_set_string_if(obj, "file_path", req->file_path);
	json_set_string_if(obj, "format", req->format);
	json_set_string_if(obj, "title", req->title);
	json_set_string_if(obj, "title_alignment", req->title_alignment);
	json_set_string_if(obj, "item_key", req->item_key);
	json_set_string_if(obj, "stdin_data", req->stdin_data);
	json_set_string_if(obj, "write_location", req->write_location);
	json_set_string_if(obj, "write_value", req->write_value);
	json_object_set_boolean(obj, "disable_auto_sleep", req->disable_auto_sleep);

	// Keyboard params
	json_set_string_if(obj, "initial_value", req->initial_value);

	// Progress params
	json_object_set_number(obj, "value", req->value);
	json_object_set_boolean(obj, "indeterminate", req->indeterminate);

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
	if (!root) return NULL;

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
	const char* cmd_str = json_object_get_string(obj, "command");
	if (cmd_str) {
		if (strcmp(cmd_str, "message") == 0) req->command = CMD_MESSAGE;
		else if (strcmp(cmd_str, "list") == 0) req->command = CMD_LIST;
		else if (strcmp(cmd_str, "keyboard") == 0) req->command = CMD_KEYBOARD;
		else if (strcmp(cmd_str, "progress") == 0) req->command = CMD_PROGRESS;
		else if (strcmp(cmd_str, "shutdown") == 0) req->command = CMD_SHUTDOWN;
	}

	req->request_id = safe_strdup(json_object_get_string(obj, "request_id"));

	// Message params
	req->message = safe_strdup(json_object_get_string(obj, "message"));
	req->timeout = json_get_int(obj, "timeout", -1);
	req->background_color = safe_strdup(json_object_get_string(obj, "background_color"));
	req->background_image = safe_strdup(json_object_get_string(obj, "background_image"));
	req->confirm_text = safe_strdup(json_object_get_string(obj, "confirm_text"));
	req->cancel_text = safe_strdup(json_object_get_string(obj, "cancel_text"));
	req->show_pill = json_get_bool(obj, "show_pill", false);
	req->show_time_left = json_get_bool(obj, "show_time_left", false);

	// List params
	req->file_path = safe_strdup(json_object_get_string(obj, "file_path"));
	req->format = safe_strdup(json_object_get_string(obj, "format"));
	req->title = safe_strdup(json_object_get_string(obj, "title"));
	req->title_alignment = safe_strdup(json_object_get_string(obj, "title_alignment"));
	req->item_key = safe_strdup(json_object_get_string(obj, "item_key"));
	req->stdin_data = safe_strdup(json_object_get_string(obj, "stdin_data"));
	req->write_location = safe_strdup(json_object_get_string(obj, "write_location"));
	req->write_value = safe_strdup(json_object_get_string(obj, "write_value"));
	req->disable_auto_sleep = json_get_bool(obj, "disable_auto_sleep", false);

	// Keyboard params
	req->initial_value = safe_strdup(json_object_get_string(obj, "initial_value"));

	// Progress params
	req->value = json_get_int(obj, "value", 0);
	req->indeterminate = json_get_bool(obj, "indeterminate", false);

	json_value_free(root);
	return req;
}

void ipc_free_request_fields(Request* req) {
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
	free(req->title_alignment);
	free(req->item_key);
	free(req->stdin_data);
	free(req->write_location);
	free(req->write_value);
	free(req->initial_value);
}

void ipc_free_request(Request* req) {
	ipc_free_request_fields(req);
	free(req);
}

int ipc_write_response(const Response* resp) {
	JSON_Value* root = json_value_init_object();
	JSON_Object* obj = json_object(root);

	json_set_string_if(obj, "request_id", resp->request_id);
	json_object_set_number(obj, "exit_code", resp->exit_code);
	json_set_string_if(obj, "output", resp->output);
	json_object_set_number(obj, "selected_index", resp->selected_index);

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
	if (!root) return NULL;

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

	resp->request_id = safe_strdup(json_object_get_string(obj, "request_id"));
	resp->exit_code = json_get_int(obj, "exit_code", EXIT_ERROR);
	resp->output = safe_strdup(json_object_get_string(obj, "output"));
	resp->selected_index = json_get_int(obj, "selected_index", -1);

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
		if (access(SHELLUI_RESPONSE_FILE, F_OK) == 0) {
			return 0;
		}

		gettimeofday(&now, NULL);
		long elapsed = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000;
		if (elapsed >= timeout_ms) {
			return -1;
		}

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
