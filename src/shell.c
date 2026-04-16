#include "shell.h"

#include "boot.h"
#include "mmu.h"
#include "uart.h"
#include "utils.h"

#define SHELL_BUFFER_SIZE 128
#define SHELL_MAX_ARGS 5
#define SHELL_DEFAULT_DUMP_WORDS 4U
#define SHELL_MAX_DUMP_WORDS 16U
#define SHELL_MAX_FILL_WORDS 64U
#define SHELL_WORD_SIZE_BYTES 4U

typedef void (*shell_command_handler_t)(int argc, char **argv);

struct shell_command {
	const char *name;
	const char *usage;
	const char *description;
	shell_command_handler_t handler;
};

static const char kShellPrompt[] = "\nSIMPL_Boot> ";

static int shell_is_whitespace(char c);
static void shell_trim(char *line);
static void shell_read_line(char *buffer, int buffer_size);
static int shell_tokenize(char *line, char **argv, int max_args);
static void shell_print_usage(const struct shell_command *command);
static const struct shell_command *shell_find_command(const char *name);
static int shell_parse_u64_argument(const char *text, uint64_t *value);
static int shell_parse_word_count(const char *text, unsigned int limit, unsigned int *count);
static int shell_validate_word_access(uint64_t address, unsigned int word_count);
static int shell_validate_dram_word_access(uint64_t address, unsigned int word_count);

static void shell_cmd_help(int argc, char **argv);
static void shell_cmd_banner(int argc, char **argv);
static void shell_cmd_version(int argc, char **argv);
static void shell_cmd_status(int argc, char **argv);
static void shell_cmd_mmu(int argc, char **argv);
static void shell_cmd_probe(int argc, char **argv);
static void shell_cmd_clear(int argc, char **argv);
static void shell_cmd_md(int argc, char **argv);
static void shell_cmd_mw(int argc, char **argv);
static void shell_cmd_fill(int argc, char **argv);

static const struct shell_command kShellCommands[] = {
	{"help", "help", "show this message", shell_cmd_help},
	{"banner", "banner", "print the default boot banner", shell_cmd_banner},
	{"version", "version", "print build metadata", shell_cmd_version},
	{"status", "status", "print execution level and MMU state", shell_cmd_status},
	{"mmu", "mmu", "dump MMU registers", shell_cmd_mmu},
	{"probe", "probe", "write/read a DRAM probe word", shell_cmd_probe},
	{"clear", "clear", "clear the terminal", shell_cmd_clear},
	{"md", "md <addr> [count]", "dump 32-bit words from mapped memory", shell_cmd_md},
	{"mw", "mw <addr> <value>", "write one 32-bit word to mapped memory", shell_cmd_mw},
	{"fill", "fill <addr> <value> <count>", "fill mapped memory with a 32-bit value", shell_cmd_fill},
};

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

static int shell_tokenize(char *line, char **argv, int max_args) {
	int argc = 0;

	while (*line != '\0') {
		while (shell_is_whitespace(*line)) {
			*line++ = '\0';
		}

		if (*line == '\0') {
			break;
		}

		if (argc >= max_args) {
			return -1;
		}

		argv[argc++] = line;
		while (*line != '\0' && !shell_is_whitespace(*line)) {
			line++;
		}
	}

	return argc;
}

static void shell_print_usage(const struct shell_command *command) {
	uart_write_string("[SHELL]: Usage: ");
	uart_write_string(command->usage);
	uart_write_string("\n");
}

static const struct shell_command *shell_find_command(const char *name) {
	size_t index;

	for (index = 0; index < sizeof(kShellCommands) / sizeof(kShellCommands[0]); index++) {
		if (strcmp(name, kShellCommands[index].name) == 0) {
			return &kShellCommands[index];
		}
	}

	return 0;
}

static int shell_parse_u64_argument(const char *text, uint64_t *value) {
	if (parse_u64(text, value) != 0) {
		uart_write_string("[SHELL]: Invalid numeric argument: ");
		uart_write_string(text);
		uart_write_string("\n");
		return -1;
	}

	return 0;
}

static int shell_parse_word_count(const char *text, unsigned int limit, unsigned int *count) {
	uint64_t parsed_count;

	if (shell_parse_u64_argument(text, &parsed_count) != 0) {
		return -1;
	}

	if (parsed_count == 0 || parsed_count > limit) {
		uart_write_string("[SHELL]: Count out of range.\n");
		return -1;
	}

	*count = (unsigned int)parsed_count;
	return 0;
}

static int shell_validate_word_access(uint64_t address, unsigned int word_count) {
	uint64_t byte_count = (uint64_t)word_count * SHELL_WORD_SIZE_BYTES;

	if ((address & (SHELL_WORD_SIZE_BYTES - 1U)) != 0U) {
		uart_write_string("[SHELL]: Address must be 32-bit aligned.\n");
		return -1;
	}

	if (!mmu_is_range_mapped(address, byte_count)) {
		uart_write_string("[SHELL]: Address range is outside the current MMU mappings.\n");
		return -1;
	}

	return 0;
}

static int shell_validate_dram_word_access(uint64_t address, unsigned int word_count) {
	uint64_t byte_count = (uint64_t)word_count * SHELL_WORD_SIZE_BYTES;
	uint64_t end_address;

	if (shell_validate_word_access(address, word_count) != 0) {
		return -1;
	}

	end_address = address + byte_count - 1ULL;
	if (address < MMU_DRAM_REGION_BASE || end_address >= MMU_DRAM_REGION_BASE + MMU_REGION_SIZE) {
		uart_write_string("[SHELL]: Writes are restricted to the DRAM mapping window.\n");
		return -1;
	}

	return 0;
}

static void shell_cmd_help(int argc, char **argv) {
	size_t index;

	(void)argc;
	(void)argv;

	uart_write_string("Commands:\n");
	for (index = 0; index < sizeof(kShellCommands) / sizeof(kShellCommands[0]); index++) {
		uart_write_string("  ");
		uart_write_string(kShellCommands[index].usage);
		uart_write_string(" - ");
		uart_write_string(kShellCommands[index].description);
		uart_write_string("\n");
	}
}

static void shell_cmd_banner(int argc, char **argv) {
	(void)argv;

	if (argc != 1) {
		shell_print_usage(&kShellCommands[1]);
		return;
	}

	boot_print_banner();
}

static void shell_cmd_version(int argc, char **argv) {
	(void)argv;

	if (argc != 1) {
		shell_print_usage(&kShellCommands[2]);
		return;
	}

	boot_print_version();
}

static void shell_cmd_status(int argc, char **argv) {
	(void)argv;

	if (argc != 1) {
		shell_print_usage(&kShellCommands[3]);
		return;
	}

	boot_print_status();
}

static void shell_cmd_mmu(int argc, char **argv) {
	(void)argv;

	if (argc != 1) {
		shell_print_usage(&kShellCommands[4]);
		return;
	}

	mmu_dump_state();
}

static void shell_cmd_probe(int argc, char **argv) {
	(void)argv;

	if (argc != 1) {
		shell_print_usage(&kShellCommands[5]);
		return;
	}

	boot_memory_probe();
}

static void shell_cmd_clear(int argc, char **argv) {
	(void)argv;

	if (argc != 1) {
		shell_print_usage(&kShellCommands[6]);
		return;
	}

	uart_write_string("\033[2J\033[H");
}

static void shell_cmd_md(int argc, char **argv) {
	uint64_t address;
	unsigned int word_count = SHELL_DEFAULT_DUMP_WORDS;
	unsigned int word_index;

	if (argc != 2 && argc != 3) {
		shell_print_usage(&kShellCommands[7]);
		return;
	}

	if (shell_parse_u64_argument(argv[1], &address) != 0) {
		return;
	}

	if (argc == 3 && shell_parse_word_count(argv[2], SHELL_MAX_DUMP_WORDS, &word_count) != 0) {
		return;
	}

	if (shell_validate_word_access(address, word_count) != 0) {
		return;
	}

	for (word_index = 0; word_index < word_count; word_index += 4U) {
		unsigned int line_index;

		uart_write_string("0x");
		uart_write_hex64(address + ((uint64_t)word_index * SHELL_WORD_SIZE_BYTES));
		uart_write_string(": ");

		for (line_index = 0; line_index < 4U && word_index + line_index < word_count; line_index++) {
			volatile uint32_t *word =
				(volatile uint32_t *)(uintptr_t)(address + ((uint64_t)(word_index + line_index) * SHELL_WORD_SIZE_BYTES));

			uart_write_hex32(*word);
			uart_write_string(" ");
		}

		uart_write_string("\n");
	}
}

static void shell_cmd_mw(int argc, char **argv) {
	uint64_t address;
	uint64_t value;
	volatile uint32_t *word;

	if (argc != 3) {
		shell_print_usage(&kShellCommands[8]);
		return;
	}

	if (shell_parse_u64_argument(argv[1], &address) != 0 || shell_parse_u64_argument(argv[2], &value) != 0) {
		return;
	}

	if (value > 0xFFFFFFFFULL) {
		uart_write_string("[SHELL]: Value must fit in 32 bits.\n");
		return;
	}

	if (shell_validate_dram_word_access(address, 1U) != 0) {
		return;
	}

	word = (volatile uint32_t *)(uintptr_t)address;
	*word = (uint32_t)value;

	uart_write_string("[SHELL]: Wrote 0x");
	uart_write_hex32(*word);
	uart_write_string(" to 0x");
	uart_write_hex64(address);
	uart_write_string("\n");
}

static void shell_cmd_fill(int argc, char **argv) {
	uint64_t address;
	uint64_t value;
	unsigned int word_count;
	unsigned int index;

	if (argc != 4) {
		shell_print_usage(&kShellCommands[9]);
		return;
	}

	if (shell_parse_u64_argument(argv[1], &address) != 0 || shell_parse_u64_argument(argv[2], &value) != 0) {
		return;
	}

	if (value > 0xFFFFFFFFULL) {
		uart_write_string("[SHELL]: Value must fit in 32 bits.\n");
		return;
	}

	if (shell_parse_word_count(argv[3], SHELL_MAX_FILL_WORDS, &word_count) != 0) {
		return;
	}

	if (shell_validate_dram_word_access(address, word_count) != 0) {
		return;
	}

	for (index = 0; index < word_count; index++) {
		volatile uint32_t *word =
			(volatile uint32_t *)(uintptr_t)(address + ((uint64_t)index * SHELL_WORD_SIZE_BYTES));
		*word = (uint32_t)value;
	}

	uart_write_string("[SHELL]: Filled count=0x");
	uart_write_hex32(word_count);
	uart_write_string(" words at 0x");
	uart_write_hex64(address);
	uart_write_string(" with 0x");
	uart_write_hex32((uint32_t)value);
	uart_write_string("\n");
}

static void shell_handle_command(char *line) {
	char *argv[SHELL_MAX_ARGS];
	int argc = shell_tokenize(line, argv, SHELL_MAX_ARGS);
	const struct shell_command *command;

	if (argc == 0) {
		return;
	}

	if (argc < 0) {
		uart_write_string("[SHELL]: Too many arguments.\n");
		return;
	}

	command = shell_find_command(argv[0]);
	if (!command) {
		uart_write_string("[SHELL]: Unknown command. Type `help`.\n");
		return;
	}

	command->handler(argc, argv);
}

void shell_run(void) {
	char buffer[SHELL_BUFFER_SIZE];

	uart_write_string("[SHELL]: Polling UART shell ready.\n");

	for (;;) {
		uart_write_string(kShellPrompt);
		shell_read_line(buffer, SHELL_BUFFER_SIZE);
		shell_handle_command(buffer);
	}
}
