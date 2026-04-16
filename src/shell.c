#include "shell.h"

#include "boot.h"
#include "mmu.h"
#include "uart.h"
#include "utils.h"

#define SHELL_BUFFER_SIZE 64

static const char kShellPrompt[] = "\nSIMPL_Boot> ";
static const char kShellHelp[] =
	"Commands:\n"
	"  help   - show this message\n"
	"  banner - print boot banner\n"
	"  status - print boot status\n"
	"  mmu    - dump MMU registers\n"
	"  probe  - write/read a DRAM probe word\n";

static int shell_is_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void shell_trim(char *line) {
	int length = strlen(line);

	while (length > 0 && shell_is_whitespace(line[length - 1])) {
		line[--length] = '\0';
	}
}

static void shell_read_line(char *buffer, int buffer_size) {
	int index = 0;

	for (;;) {
		char c = uart_read_char();

		if (c == '\r' || c == '\n') {
			uart_write_string("\n");
			break;
		}

		if (c == '\b' || c == 127) {
			if (index > 0) {
				index--;
				uart_write_string("\b \b");
			}
			continue;
		}

		if (index < buffer_size - 1) {
			buffer[index++] = c;
			uart_write_char(c);
		}
	}

	buffer[index] = '\0';
	shell_trim(buffer);
}

static void shell_handle_command(const char *command) {
	if (strcmp(command, "help") == 0) {
		uart_write_string(kShellHelp);
		return;
	}

	if (strcmp(command, "banner") == 0) {
		boot_print_banner();
		return;
	}

	if (strcmp(command, "status") == 0) {
		boot_print_status();
		return;
	}

	if (strcmp(command, "mmu") == 0) {
		mmu_dump_state();
		return;
	}

	if (strcmp(command, "probe") == 0) {
		boot_memory_probe();
		return;
	}

	uart_write_string("[SHELL]: Unknown command. Type `help`.\n");
}

void shell_run(void) {
	char buffer[SHELL_BUFFER_SIZE];

	uart_write_string("[SHELL]: Polling UART shell ready.\n");

	for (;;) {
		uart_write_string(kShellPrompt);
		shell_read_line(buffer, SHELL_BUFFER_SIZE);

		if (buffer[0] == '\0') {
			continue;
		}

		shell_handle_command(buffer);
	}
}
