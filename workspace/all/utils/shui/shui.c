/**
 * shui - Persistent UI daemon for shell scripts
 *
 * Single binary that operates in two modes:
 * - CLI mode: Sends commands to daemon, auto-starts if needed
 * - Daemon mode: Keeps SDL initialized, processes UI requests
 *
 * Usage:
 *   shui message "text" [--timeout N] [--confirm TEXT] [--cancel TEXT]
 *   shui list --file FILE [--format json|text] [--title TEXT]
 *   shui keyboard [--title TEXT] [--initial TEXT]
 *   shui stop
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "ipc.h"

// Platform includes for daemon mode
#ifdef PLATFORM
#include "api.h"
#include "defines.h"
#include "fonts.h"
#include "log.h"
#include "shui_utils.h"
#include "ui_message.h"
#include "ui_list.h"
#include "ui_keyboard.h"
#include "ui_progress.h"
#include <msettings.h>
#ifdef USE_SDL2
#include <SDL2/SDL_ttf.h>
#else
#include <SDL/SDL_ttf.h>
#endif
#endif

// Forward declarations
static int run_cli(int argc, char** argv);
static int run_daemon(void);
static int daemon_is_running(void);
static int daemon_spawn(void);
static int daemon_wait_ready(int timeout_ms);
static int send_command(const Request* req);

// Global state for daemon mode
static volatile sig_atomic_t daemon_quit = 0;
#ifdef PLATFORM
static SDL_Surface* screen = NULL;

// State for progress UI (persists between calls for animation)
static ProgressState progress_state = {0};
static ProgressOptions current_progress_opts = {0};
#endif

// Signal handler
static void signal_handler(int sig) {
	if (sig == SIGTERM || sig == SIGINT) {
		daemon_quit = 1;
	}
}

// Read all of stdin into a string (for piped input)
static char* read_stdin_all(void) {
	// Check if stdin has data (is a pipe or file, not a tty)
	if (isatty(STDIN_FILENO)) {
		return NULL;
	}

	size_t capacity = 4096;
	size_t size = 0;
	char* buffer = malloc(capacity);
	if (!buffer) return NULL;

	ssize_t n;
	while ((n = read(STDIN_FILENO, buffer + size, capacity - size - 1)) > 0) {
		size += n;
		if (size >= capacity - 1) {
			capacity *= 2;
			char* newbuf = realloc(buffer, capacity);
			if (!newbuf) {
				free(buffer);
				return NULL;
			}
			buffer = newbuf;
		}
	}

	buffer[size] = '\0';
	return buffer;
}

static void print_usage(void) {
	fprintf(stderr,
		"Usage: shui <command> [options]\n"
		"\n"
		"Commands:\n"
		"  message TEXT      Show a message dialog\n"
		"  list              Show a list selector\n"
		"  keyboard          Show keyboard input\n"
		"  progress TEXT     Show a progress bar\n"
		"  auto-sleep <on|off>  Control device auto-sleep\n"
		"  restart           Start daemon or reset session state\n"
		"  start             Start the daemon (for pre-warming)\n"
		"  stop              Stop the daemon\n"
		"\n"
		"Message options:\n"
		"  --timeout N       Auto-dismiss after N seconds (-1 = forever)\n"
		"  --confirm TEXT    Confirm button label (A button)\n"
		"  --cancel TEXT     Cancel button label (B button)\n"
		"  --subtext TEXT    Secondary text below main message\n"
		"  --background-color #RRGGBB\n"
		"  --background-image PATH\n"
		"  --show-pill       Show pill background around text\n"
		"\n"
		"List options:\n"
		"  --file PATH       JSON or text file with items\n"
		"  --format FORMAT   'json' or 'text' (default: json)\n"
		"  --title TEXT      Dialog title\n"
		"  --item-key KEY    JSON array key (default: items)\n"
		"  --confirm TEXT    Confirm button label (A button)\n"
		"  --cancel TEXT     Cancel button label (B button)\n"
		"  --write-location  Write output to file instead of stdout\n"
		"  --write-value     Output type: selected, state, name, value\n"
		"\n"
		"Keyboard options:\n"
		"  --title TEXT      Prompt title\n"
		"  --initial TEXT    Initial input value\n"
		"  --write-location  Write output to file instead of stdout\n"
		"\n"
		"Progress options:\n"
		"  --value N         Progress percentage (0-100)\n"
		"  --indeterminate   Show animated bar instead of fixed\n"
		"  --title TEXT      Title above progress bar\n"
		"  --subtext TEXT    Secondary text below main message\n"
		"\n"
		"Output is written to stdout. Exit codes:\n"
		"  0 = Success, 2 = Cancel, 3 = Menu, 124 = Timeout\n"
	);
}

int main(int argc, char** argv) {
	// Check for --daemon flag (internal use only)
	if (argc >= 2 && strcmp(argv[1], "--daemon") == 0) {
		return run_daemon();
	}

	return run_cli(argc, argv);
}

// ============================================================================
// CLI Mode
// ============================================================================

static int run_cli(int argc, char** argv) {
	if (argc < 2) {
		print_usage();
		return EXIT_ERROR;
	}

	const char* cmd = argv[1];
	Request req = {0};
	req.request_id = ipc_generate_request_id();
	req.timeout = -1;  // default: no timeout

	// Parse command
	if (strcmp(cmd, "message") == 0) {
		req.command = CMD_MESSAGE;
	} else if (strcmp(cmd, "list") == 0) {
		req.command = CMD_LIST;
		req.format = strdup("json");
		req.item_key = strdup("items");
	} else if (strcmp(cmd, "keyboard") == 0) {
		req.command = CMD_KEYBOARD;
	} else if (strcmp(cmd, "progress") == 0) {
		req.command = CMD_PROGRESS;
	} else if (strcmp(cmd, "start") == 0) {
		req.command = CMD_START;
	} else if (strcmp(cmd, "stop") == 0) {
		req.command = CMD_SHUTDOWN;
	} else if (strcmp(cmd, "auto-sleep") == 0) {
		req.command = CMD_AUTO_SLEEP;
		// Requires "on" or "off" argument
		if (argc < 3) {
			fprintf(stderr, "Usage: shui auto-sleep <on|off>\n");
			free(req.request_id);
			return EXIT_ERROR;
		}
		if (strcmp(argv[2], "on") == 0) {
			req.value = 1;
		} else if (strcmp(argv[2], "off") == 0) {
			req.value = 0;
		} else {
			fprintf(stderr, "Usage: shui auto-sleep <on|off>\n");
			free(req.request_id);
			return EXIT_ERROR;
		}
	} else if (strcmp(cmd, "restart") == 0) {
		req.command = CMD_RESTART;
	} else if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
		print_usage();
		free(req.request_id);
		return EXIT_SUCCESS_CODE;
	} else {
		fprintf(stderr, "Unknown command: %s\n", cmd);
		print_usage();
		free(req.request_id);
		return EXIT_ERROR;
	}

	// Parse options
	static struct option long_options[] = {
		// Message options
		{"timeout", required_argument, 0, 't'},
		{"confirm", required_argument, 0, 'c'},
		{"cancel", required_argument, 0, 'x'},
		{"subtext", required_argument, 0, 's'},
		{"background-color", required_argument, 0, 'b'},
		{"background-image", required_argument, 0, 'B'},
		{"show-pill", no_argument, 0, 'p'},
		// List options
		{"file", required_argument, 0, 'f'},
		{"format", required_argument, 0, 'F'},
		{"title", required_argument, 0, 'T'},
		{"title-alignment", required_argument, 0, 'L'},
		{"item-key", required_argument, 0, 'k'},
		{"write-location", required_argument, 0, 'w'},
		{"write-value", required_argument, 0, 'W'},
		// Keyboard options
		{"initial", required_argument, 0, 'i'},
		// Progress options
		{"value", required_argument, 0, 'v'},
		{"indeterminate", no_argument, 0, 'I'},
		// Help
		{"help", no_argument, 0, 'h'},
		{0, 0, 0, 0}
	};

	optind = 2;  // Skip program name and command
	int opt;
	while ((opt = getopt_long(argc, argv, "t:c:x:s:b:B:pf:F:T:L:k:w:W:i:v:Ih", long_options, NULL)) != -1) {
		switch (opt) {
			case 't':
				req.timeout = atoi(optarg);
				break;
			case 'c':
				req.confirm_text = strdup(optarg);
				break;
			case 'x':
				req.cancel_text = strdup(optarg);
				break;
			case 's':
				req.subtext = strdup(optarg);
				break;
			case 'b':
				req.background_color = strdup(optarg);
				break;
			case 'B':
				req.background_image = strdup(optarg);
				break;
			case 'p':
				req.show_pill = true;
				break;
			case 'f':
				req.file_path = strdup(optarg);
				break;
			case 'F':
				free(req.format);
				req.format = strdup(optarg);
				break;
			case 'T':
				req.title = strdup(optarg);
				break;
			case 'L':
				req.title_alignment = strdup(optarg);
				break;
			case 'k':
				free(req.item_key);
				req.item_key = strdup(optarg);
				break;
			case 'w':
				req.write_location = strdup(optarg);
				break;
			case 'W':
				req.write_value = strdup(optarg);
				break;
			case 'i':
				req.initial_value = strdup(optarg);
				break;
			case 'v':
				req.value = atoi(optarg);
				break;
			case 'I':
				req.indeterminate = true;
				break;
			case 'h':
				print_usage();
				ipc_free_request_fields(&req);
				return EXIT_SUCCESS_CODE;
			default:
				break;
		}
	}

	// Get positional argument (message text for message/progress commands)
	if ((req.command == CMD_MESSAGE || req.command == CMD_PROGRESS) && optind < argc) {
		req.message = strdup(argv[optind]);
	}

	// Read stdin for piped list input
	if (req.command == CMD_LIST && !req.file_path) {
		req.stdin_data = read_stdin_all();
	}

	// Validate
	if (req.command == CMD_MESSAGE && !req.message) {
		fprintf(stderr, "Error: message command requires text argument\n");
		ipc_free_request_fields(&req);
		return EXIT_ERROR;
	}
	if (req.command == CMD_PROGRESS && !req.message) {
		fprintf(stderr, "Error: progress command requires text argument\n");
		ipc_free_request_fields(&req);
		return EXIT_ERROR;
	}

	// Send command
	int result = send_command(&req);

	// Cleanup (req is stack-allocated, only free fields)
	ipc_free_request_fields(&req);

	return result;
}

static int daemon_is_running(void) {
	FILE* f = fopen(SHUI_PID_FILE, "r");
	if (!f) return 0;

	pid_t pid;
	if (fscanf(f, "%d", &pid) != 1) {
		fclose(f);
		return 0;
	}
	fclose(f);

	// Check if process exists
	if (kill(pid, 0) == 0) {
		return 1;
	}

	// Stale PID file
	unlink(SHUI_PID_FILE);
	return 0;
}

static int daemon_spawn(void) {
	// Create directory first (needed for lock file)
	if (mkdir(SHUI_DIR, 0755) != 0 && errno != EEXIST) {
		perror("mkdir");
		return -1;
	}

	// Acquire exclusive lock to prevent concurrent spawn attempts
	int lock_fd = open(SHUI_LOCK_FILE, O_CREAT | O_RDWR, 0644);
	if (lock_fd < 0) {
		perror("open lock file");
		return -1;
	}

	if (flock(lock_fd, LOCK_EX) < 0) {
		perror("flock");
		close(lock_fd);
		return -1;
	}

	// Re-check after acquiring lock - another process may have spawned daemon
	if (daemon_is_running()) {
		close(lock_fd);  // Releases lock
		return daemon_wait_ready(DAEMON_STARTUP_TIMEOUT_MS);
	}

	// We hold the lock and daemon is not running - safe to spawn
	// Clean up old IPC files
	ipc_cleanup();
	ipc_init();

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		close(lock_fd);
		return -1;
	}

	if (pid == 0) {
		// Child: become daemon (lock fd inherited, closed on exec)
		close(lock_fd);
		setsid();

		// Re-exec ourselves with --daemon flag
		char* self = "/proc/self/exe";
		char path[512];
		ssize_t len = readlink(self, path, sizeof(path) - 1);
		if (len > 0) {
			path[len] = '\0';
			execl(path, "shui", "--daemon", NULL);
		}

		// Fallback: just run daemon directly (shouldn't reach here in normal cases)
		exit(run_daemon());
	}

	// Parent: wait for daemon to be ready, then release lock
	int result = daemon_wait_ready(DAEMON_STARTUP_TIMEOUT_MS);
	close(lock_fd);  // Releases lock
	return result;
}

static int daemon_wait_ready(int timeout_ms) {
	struct timeval start, now;
	gettimeofday(&start, NULL);

	while (1) {
		if (access(SHUI_READY_FILE, F_OK) == 0) {
			return 0;
		}

		gettimeofday(&now, NULL);
		long elapsed = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000;
		if (elapsed >= timeout_ms) {
			fprintf(stderr, "Timeout waiting for daemon to start\n");
			return -1;
		}

		usleep(10000);  // 10ms
	}
}

// Check if this request needs to wait for a response
static bool request_needs_response(const Request* req) {
	// Messages without buttons don't need a response - fire and forget
	if (req->command == CMD_MESSAGE) {
		bool has_buttons = (req->confirm_text && strlen(req->confirm_text) > 0) ||
		                   (req->cancel_text && strlen(req->cancel_text) > 0);
		return has_buttons;
	}

	// Shutdown doesn't need a response
	if (req->command == CMD_SHUTDOWN) {
		return false;
	}

	// Progress is fire-and-forget
	if (req->command == CMD_PROGRESS) {
		return false;
	}

	// Auto-sleep and restart are fire-and-forget
	if (req->command == CMD_AUTO_SLEEP || req->command == CMD_RESTART) {
		return false;
	}

	// List and keyboard always need responses
	return true;
}

static int send_command(const Request* req) {
	// Special case: stop always cleans up (handles crash recovery)
	if (req->command == CMD_SHUTDOWN) {
		if (!daemon_is_running()) {
			ipc_cleanup();  // Remove any stale files from crash
			return EXIT_SUCCESS_CODE;
		}
	}

	// Ensure daemon is running
	if (!daemon_is_running()) {
		if (daemon_spawn() != 0) {
			fprintf(stderr, "Failed to start daemon\n");
			return EXIT_ERROR;
		}
	}

	// Special case: start just ensures daemon is running, no command needed
	if (req->command == CMD_START) {
		return EXIT_SUCCESS_CODE;
	}

	// Clean up any stale response from previous fire-and-forget command
	ipc_delete_response();

	// Write request
	if (ipc_write_request(req) != 0) {
		fprintf(stderr, "Failed to write request\n");
		return EXIT_ERROR;
	}

	// Fire-and-forget commands: don't wait for response
	if (!request_needs_response(req)) {
		return EXIT_SUCCESS_CODE;
	}

	// Wait for response
	if (ipc_wait_for_response(RESPONSE_TIMEOUT_MS) != 0) {
		fprintf(stderr, "Timeout waiting for response\n");
		return EXIT_TIMEOUT;
	}

	// Read response
	Response* resp = ipc_read_response();
	if (!resp) {
		fprintf(stderr, "Failed to read response\n");
		return EXIT_ERROR;
	}

	// Output result to stdout
	if (resp->output && strlen(resp->output) > 0) {
		printf("%s\n", resp->output);
	}

	int exit_code = resp->exit_code;
	ipc_delete_response();
	ipc_free_response(resp);

	return exit_code;
}

// ============================================================================
// Daemon Mode
// ============================================================================

#ifdef PLATFORM

// Suppress stdout during init (some platforms print debug info)
static int saved_stdout = -1;
static int saved_stderr = -1;

static void suppress_output(void) {
	fflush(stdout);
	fflush(stderr);
	saved_stdout = dup(STDOUT_FILENO);
	saved_stderr = dup(STDERR_FILENO);
	int devnull = open("/dev/null", O_WRONLY);
	if (devnull >= 0) {
		dup2(devnull, STDOUT_FILENO);
		dup2(devnull, STDERR_FILENO);
		close(devnull);
	}
}

static void restore_output(void) {
	fflush(stdout);
	fflush(stderr);
	if (saved_stdout >= 0) {
		dup2(saved_stdout, STDOUT_FILENO);
		close(saved_stdout);
		saved_stdout = -1;
	}
	if (saved_stderr >= 0) {
		dup2(saved_stderr, STDERR_FILENO);
		close(saved_stderr);
		saved_stderr = -1;
	}
}

static void daemon_init(void) {
	LOG_debug("daemon_init");
	if (screen == NULL) {
		LOG_debug("GFX_init");
		screen = GFX_init(MODE_MAIN);
		if (screen == NULL) {
			LOG_error("GFX_init failed");
			return;
		}
	}
	LOG_debug("PAD_init");
	PAD_init();
	LOG_debug("PWR_init");
	PWR_init();
	LOG_debug("InitSettings");
	InitSettings();
	LOG_debug("fonts_init");
	fonts_init();
	LOG_debug("daemon_init complete");
}

static void daemon_cleanup(void) {
	fonts_cleanup();
	QuitSettings();
	PWR_quit();
	PAD_quit();
	GFX_quit();
	screen = NULL;
}

// Render a simple status message (non-blocking, fire-and-forget)
static void render_status_message(const char* text) {
	if (!screen || !text || !g_font_large) return;

	GFX_clear(screen);

	// Process escape sequences
	char processed[1024];
	unescapeNewlines(processed, text, sizeof(processed));

	// Simple centered message
	SDL_Surface* msg = TTF_RenderUTF8_Blended(g_font_large, processed, COLOR_WHITE);
	if (msg) {
		int x = (screen->w - msg->w) / 2;
		int y = (screen->h - msg->h) / 2;
		SDL_Rect pos = {x, y, msg->w, msg->h};
		SDL_BlitSurface(msg, NULL, screen, &pos);
		SDL_FreeSurface(msg);
	}

	GFX_flip(screen);
}

// Handle message command using ui_message module
static ExitCode handle_message(const Request* req, bool wait_for_response) {
	// Fire-and-forget: just render and return
	if (!wait_for_response) {
		render_status_message(req->message);
		return EXIT_SUCCESS_CODE;
	}

	// Interactive: full ui_message_show with event loop
	MessageOptions opts = {
		.text = req->message,
		.subtext = req->subtext,
		.timeout = req->timeout,
		.background_color = req->background_color,
		.background_image = req->background_image,
		.confirm_text = req->confirm_text,
		.cancel_text = req->cancel_text,
		.show_pill = req->show_pill,
		.show_time_left = false,
	};
	return ui_message_show(screen, &opts);
}

// Handle list command using ui_list module
static void handle_list(const Request* req, Response* resp) {
	// Parse items from file, stdin_data, or treat as single item
	ListItem* items = NULL;
	int item_count = 0;
	const char* format = req->format ? req->format : "json";
	const char* item_key = req->item_key ? req->item_key : "items";

	// Try file first
	if (req->file_path) {
		FILE* f = fopen(req->file_path, "r");
		if (f) {
			fseek(f, 0, SEEK_END);
			long size = ftell(f);
			fseek(f, 0, SEEK_SET);
			char* content = malloc(size + 1);
			if (content) {
				fread(content, 1, size, f);
				content[size] = '\0';

				if (strcmp(format, "text") == 0) {
					items = ui_list_parse_text(content, &item_count);
				} else {
					items = ui_list_parse_json(content, item_key, &item_count);
				}
				free(content);
			}
			fclose(f);
		}
	}
	// Try stdin_data
	else if (req->stdin_data) {
		if (strcmp(format, "text") == 0) {
			items = ui_list_parse_text(req->stdin_data, &item_count);
		} else {
			items = ui_list_parse_json(req->stdin_data, item_key, &item_count);
		}
	}

	if (!items || item_count == 0) {
		resp->exit_code = EXIT_ERROR;
		resp->output = strdup("No items to display");
		return;
	}

	ListOptions opts = {
		.title = req->title,
		.title_alignment = req->title_alignment,
		.confirm_text = req->confirm_text,
		.cancel_text = req->cancel_text,
		.write_location = req->write_location,
		.write_value = req->write_value,
		.items = items,
		.item_count = item_count,
		.initial_index = 0,
	};

	ListResult result = ui_list_show(screen, &opts);
	resp->exit_code = result.exit_code;
	resp->selected_index = result.selected_index;

	// Handle write_value output
	if (req->write_value && strcmp(req->write_value, "state") == 0) {
		resp->output = result.state_json;
		free(result.selected_value);
	} else {
		resp->output = result.selected_value;
		free(result.state_json);
	}

	// Handle write_location (write to file instead of stdout)
	if (req->write_location && strcmp(req->write_location, "-") != 0 && resp->output) {
		FILE* f = fopen(req->write_location, "w");
		if (f) {
			fputs(resp->output, f);
			fclose(f);
		}
	}

	ui_list_free_items(items, item_count);
}

static void process_request(const Request* req, Response* resp) {
	resp->request_id = req->request_id ? strdup(req->request_id) : NULL;
	resp->output = NULL;

	// Reset progress state when switching to a different UI
	if (req->command != CMD_PROGRESS) {
		ui_progress_reset(&progress_state);
		free(current_progress_opts.message);
		free(current_progress_opts.title);
		free(current_progress_opts.subtext);
		current_progress_opts.message = NULL;
		current_progress_opts.title = NULL;
		current_progress_opts.subtext = NULL;
	}

	switch (req->command) {
		case CMD_MESSAGE: {
			// Check if message has buttons
			bool has_buttons = (req->confirm_text && strlen(req->confirm_text) > 0) ||
			                   (req->cancel_text && strlen(req->cancel_text) > 0);
			resp->exit_code = handle_message(req, has_buttons);
			break;
		}

		case CMD_LIST:
			handle_list(req, resp);
			break;

		case CMD_KEYBOARD: {
			KeyboardOptions kb_opts = {
				.title = req->title,
				.initial_value = req->initial_value,
			};
			KeyboardResult kb_result = ui_keyboard_show(screen, &kb_opts);
			resp->exit_code = kb_result.exit_code;
			resp->output = kb_result.text;

			// Handle write_location
			if (req->write_location && strcmp(req->write_location, "-") != 0 && resp->output) {
				FILE* f = fopen(req->write_location, "w");
				if (f) {
					fputs(resp->output, f);
					fclose(f);
				}
			}
			break;
		}

		case CMD_PROGRESS: {
			// Free previous copies
			free(current_progress_opts.message);
			free(current_progress_opts.title);
			free(current_progress_opts.subtext);

			// Store copies of options for rendering (must outlive request)
			current_progress_opts.message = req->message ? strdup(req->message) : NULL;
			current_progress_opts.title = req->title ? strdup(req->title) : NULL;
			current_progress_opts.subtext = req->subtext ? strdup(req->subtext) : NULL;
			current_progress_opts.value = req->value;
			current_progress_opts.indeterminate = req->indeterminate;

			// Update state (handles context matching and animation setup)
			ui_progress_update(&progress_state, &current_progress_opts);

			// Render immediately
			ui_progress_render(screen, &progress_state, &current_progress_opts);
			resp->exit_code = EXIT_SUCCESS_CODE;
			break;
		}

		case CMD_SHUTDOWN:
			PWR_enableAutosleep();
			resp->exit_code = EXIT_SUCCESS_CODE;
			daemon_quit = 1;
			break;

		case CMD_AUTO_SLEEP:
			if (req->value) {
				PWR_enableAutosleep();
			} else {
				PWR_disableAutosleep();
			}
			resp->exit_code = EXIT_SUCCESS_CODE;
			break;

		case CMD_RESTART:
			// Reset all session state
			PWR_enableAutosleep();
			ui_progress_reset(&progress_state);
			free(current_progress_opts.message);
			free(current_progress_opts.title);
			free(current_progress_opts.subtext);
			current_progress_opts.message = NULL;
			current_progress_opts.title = NULL;
			current_progress_opts.subtext = NULL;
			resp->exit_code = EXIT_SUCCESS_CODE;
			break;

		default:
			resp->exit_code = EXIT_ERROR;
			break;
	}
}

static int run_daemon(void) {
	// Initialize logging early (reads LOG_FILE and LOG_SYNC from environment)
	log_open(NULL);

	LOG_info("Starting shui daemon on %s", PLATFORM);

	// Setup signal handling
	LOG_debug("Setting up signal handlers");
	struct sigaction sa = {
		.sa_handler = signal_handler,
		.sa_flags = SA_RESTART
	};
	sigemptyset(&sa.sa_mask);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	// Write PID file
	LOG_debug("Writing PID file");
	ipc_init();
	FILE* f = fopen(SHUI_PID_FILE, "w");
	if (f) {
		fprintf(f, "%d", getpid());
		fclose(f);
	}

	// Initialize graphics
	suppress_output();
	daemon_init();
	restore_output();

	// Signal that we're ready
	LOG_debug("Writing ready file");
	f = fopen(SHUI_READY_FILE, "w");
	if (f) fclose(f);

	LOG_debug("Entering main loop");
	// Main loop: wait for requests
	while (!daemon_quit) {
		// Handle power button even when idle (NULLs: no redraw tracking,
		// no settings overlay, no sleep callbacks - we're not rendering)
		PAD_poll();
		PWR_update(NULL, NULL, NULL, NULL);

		// Check for request file
		if (access(SHUI_REQUEST_FILE, F_OK) == 0) {
			Request* req = ipc_read_request();
			if (req) {
				ipc_delete_request();

				Response resp = {0};
				process_request(req, &resp);

				ipc_write_response(&resp);

				free(resp.request_id);
				free(resp.output);
				ipc_free_request(req);
			}
		}

		// Animate progress bar (indeterminate or value transition)
		if (ui_progress_needs_animation(&progress_state)) {
			ui_progress_render(screen, &progress_state, &current_progress_opts);
		}

		// Brief sleep to avoid busy-waiting
		usleep(16000);  // ~60fps
	}

	// Cleanup
	suppress_output();
	daemon_cleanup();
	restore_output();

	ipc_cleanup();

	// Close log file (flushes and syncs to disk)
	log_close();

	return EXIT_SUCCESS_CODE;
}

#else
// Non-platform build (for testing CLI on host)
static int run_daemon(void) {
	fprintf(stderr, "Daemon mode requires platform build\n");
	return EXIT_ERROR;
}
#endif
