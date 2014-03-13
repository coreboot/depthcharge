/*
 * This submission is loosely based on
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2014 Chromium OS Authors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "debug/cli/common.h"
#include "debug/cli/command.h"

#define DEBUG_PARSER	0

char console_buffer[MAX_CONSOLE_LINE + 1]; /* console I/O buffer */

#define debug_parser(fmt, args...)		\
	debug_cond(DEBUG_PARSER, fmt, ##args)

#define ESC 0x1b

/* Cursor movement commands characters. */
#define CHAR_HOME	   1 /* ^A */
#define CHAR_CTL	   3 /* ^C */
#define CHAR_EOL	   5 /* ^E */
#define CHAR_RIGHT	  12 /* ^L */
#define CHAR_DEL	0x7f
#define CHAR_LEFT	0x81 /* here and below: ficticious */
#define CHAR_UP		0x82
#define CHAR_DOWN	0x83

static const char erase_seq[] = "\b \b";

static void move_cursor_left(int positions)
{
	const char cursor_left[] = { ESC, '[', 'D', '\0' };

	while (positions-- > 0)
		printf("%s", cursor_left);
}

static void move_cursor_right(int positions)
{
	const char cursor_left[] = { ESC, '[', 'C', '\0' };

	while (positions-- > 0)
		printf("%s", cursor_left);
}

/***************************************************************************/
/*vvvvvvvvvvvvvvvvvvvvvvvv  History support vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv*/

typedef struct {
	char console_string[MAX_CONSOLE_LINE + 1];
	int  cursor;
} history_node;

/*
 * 20 history entries should be enough.
 *
 * history_bottom is were the next history item will be added, history_spot is
 * where the user currently is in the history array.
 */
static history_node history[20];
static int history_bottom, history_spot;

#define HISTORY_NEXT(x) (((x) + 1) % ARRAY_SIZE(history))
#define HISTORY_PREV(x) ((x) ? ((x) - 1) : ARRAY_SIZE(history) - 1)

static void history_add(const char *string)
{
	history_node *hnode;

	if (!strcmp(string,
		    history[HISTORY_PREV(history_bottom)].console_string))
		return; /* no need to duplicate strings in history */

	hnode = history + history_bottom;

	strncpy(hnode->console_string,
		string,
		sizeof(hnode->console_string));

	/* make sure it is null terminated no matter what */
	hnode->console_string[sizeof(hnode->console_string) - 1] = '\0';
	hnode->cursor = strlen(hnode->console_string);

	history_spot = history_bottom = HISTORY_NEXT(history_bottom);
	history[history_bottom].console_string[0] = '\0';
}

/* The user hit a history browsing button. */
static void history_case(u8 c, char *p_buf, int *np, int *cursor_p)
{
	int n = *np, cursor = *cursor_p, new_n, tmp;

	if ((c == CHAR_DOWN) && (history_bottom == history_spot))
		return; /* we are already at the bottom of history */

	if ((c == CHAR_UP) &&
	    (!history[HISTORY_PREV(history_spot)].console_string[0]))
		return; /* we are already at the top of history */

	if (history_bottom == history_spot) {
		/* we are in a new console line, save it into history */
		if (n)
			history_add(p_buf);
	}

	/*
	 * Just in case user was editing the line and it the cursor is now
	 * beyond the saved line's length.
	 */
	tmp = strlen(history[history_spot].console_string);
	if (tmp > cursor)
		tmp = cursor;

	history[history_spot].cursor = tmp;
	history_spot = (c == CHAR_UP) ? HISTORY_PREV(history_spot) :
		HISTORY_NEXT(history_spot);

	/* Get to the first column */
	move_cursor_left(cursor);

	/* Copy new string into the console buffer and print it on the screen */
	strcpy(p_buf, history[history_spot].console_string);
	new_n = printf("%s", p_buf);

	/* erase the rest of the line if needed */
	tmp = n - new_n;
	while (tmp-- > 0)
		printf(" ");

	/* come back to the end of the line */
	move_cursor_left(n - new_n);
	*np = new_n;
	*cursor_p = history[history_spot].cursor;

	/* move screen cursor into position */
	move_cursor_left(new_n - *cursor_p);
}

#ifdef HISTORY_DEBUG
static int do_dh(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int i;

	printf("bottom at %d, spot at %d\n", history_bottom, history_spot);
	for (i = HISTORY_PREV(history_bottom);
	     i != history_bottom;
	     i = HISTORY_PREV(i))
		if (history[i].console_string[0])
			printf("%2.2d %s\n", i, history[i].console_string);

	return 0;
}

U_BOOT_CMD(
	dh, 1,	1,
	"commamd to support history debugging", NULL
);
#endif

static int console_done;

static int do_exit(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	console_done = 1;
	return 0;
}

U_BOOT_CMD(
	exit, 1, 1,
	"exit command console to coninue normal boot", NULL
);

/*^^^^^^^^^^^^^^^^^^^^^^^^  History support ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
/***************************************************************************/

static void backspace(char *buffer, int *np, int *cp)
{
	int n, cursor, shift;

	n = *np;

	if (!n) {
		return;
	}

	cursor = *cp;

	if (n == cursor) {
		printf(erase_seq);
		*np = --n;
		*cp = --cursor;
		buffer[n] = '\0';
		return;
	}

	if (!cursor)
		return;

	printf(erase_seq);
	shift = n - cursor;
	memcpy(buffer + cursor - 1, buffer + cursor, shift);
	buffer[n - 1] = ' ';
	buffer[n] = '\0';
	printf("%s", buffer + cursor - 1);
	move_cursor_left(shift + 1);
	n -= 1;
	buffer[n] = '\0';
	*np = n;
	*cp = cursor - 1;
}


/*
 * We want to support function keys for command line editing. Many of those
 * keys on a standard xterm generate escape sequences. The code below
 * implements a state machine to map known escape sequences into appropriate
 * singe keystrokes.
 *
 * Some of the keys are processed but ignored.
 */
/* Map an escape sequence into a single byte ensuring the same action */
typedef struct {
	const char *chars;  /* symbols of the sequence (escape not included */
	char map_to;	    /* keystroke the sequence maps to (if any) */
} escaped_key;


static const escaped_key  escaped_keys [] = {
	{ "OF", CHAR_EOL},
	{ "[1~", CHAR_HOME},
	{ "[2~"}, /* insert */
	{ "[3~", CHAR_DEL},
	{ "[5~"}, /* page up */
	{ "[6~"}, /* page down */
	{ "[A", CHAR_UP},
	{ "[B", CHAR_DOWN},
	{ "[C", CHAR_RIGHT},
	{ "[D", CHAR_LEFT},
	{ "zzzz" } /* will never happen */
};

/*
 * Implement state machine discovering escape sequences listed above.
 *
 * Input parameter is a pointer to the next character received on the console.
 *
 * If the character is not a part of an escape sequence, return false so that
 * the character can be processed as a regular symbol.
 *
 * If the character is a part of the incoming escape sequence, return true, as
 * no further processing is required.
 *
 * If the sequence is not recognised - return false so that the last character
 * of the unrecognised sequence is processed as a regular symbol.
 *
 * If the sequence is recognised and has a mapping defined - replace the input
 * character with the map value and return false so that the replacement is
 * processed as if received from the console.
 *
 * If no mapping is defined - return true so that the sequence is ignored.
 */
static int escape_handled(u8 *cp)
{
	u8 c = *cp;
	/* pointer to the row matching the sequence so far */
	static const escaped_key *this_key;
	/*
	 * Position of the next expected character in the sequence, -1 means
	 * we are not in a sequence yet.
	 */
	static int escape_sn = -1;

	if (escape_sn < 0) {
		if (c == ESC) {
			escape_sn = 0; /* got the escape */
			this_key = escaped_keys;
			return 1;
		}  else {
			return 0;
		}
	}

	if (this_key->chars[escape_sn] != c) {
		int on_track = 0;

		/*
		 * We are not in the right row of the table. Maybe some next
		 * one is better? If we are not on the first symbol of the
		 * sequence we can proceed only if the previous characters in
		 * this and the next rows are the same.
		 */
		while ((!escape_sn) ||
		       (this_key[0].chars[escape_sn - 1] ==
			this_key[1].chars[escape_sn - 1])) {
			this_key++;
			if (this_key->chars[escape_sn] == c) {
				on_track = 1;
				break;
			}
		}

		if (!on_track) {
			/*
			 * This is an unexpected escape sequence, consider it
			 * finished with this character.
			 */
			escape_sn = -1;
			return 1;
		}
	}

	if (this_key->chars[++escape_sn] == '\0') {
		/* end of escape sequence */
		escape_sn = -1;

		if (this_key->map_to) {
			*cp = this_key->map_to;
			return 0;
		}
	}
	return 1;
}

/*
 * Erase the word at cursor in the passed in buffer. The words are delimited
 * by space characters.
 *
 * Inputs:
 *  p_buf     - pointer to the buffer containting the string
 *  np        - pointer to the number of characters in the buffer
 *  cursor_p - pointer to the curcor index in the buffer
 *
 * Once the word is erased, update number of characters remaining in the
 * buffer and the cursor position.
 */
static void erase_word(char *p_buf, int *np, int *cursor_p)
{
	int word_end, word_start;
	int n = *np;
	int cursor = *cursor_p;

	if (cursor == n) {
		do {
			backspace(p_buf, np, cursor_p);
			n = *np;
		} while ((n > 0) && (p_buf[n - 1] != ' '));

		if (n)
			backspace(p_buf, np, cursor_p);
		return;
	}

	word_start = word_end = cursor;

	while ((word_end < n) &&
	       (p_buf[word_end] != ' '))
		word_end++;

	while ((word_start > 0) &&
	       (p_buf[--word_start] != ' '))
		;

	move_cursor_left(cursor - word_start);

	cursor = word_start;

	if (!word_start && (word_end != n))
		word_end++;

	memcpy(p_buf + word_start, p_buf + word_end, n - word_end);
	memset(p_buf + n - word_end + word_start, ' ', word_end - word_start);
	printf("%s", p_buf + cursor);

	move_cursor_left(n - cursor);

	n -= word_end - word_start;
	p_buf[n] = '\0';

	*cursor_p = word_start;
	*np = n;
}

/*
 * Prompt for input and read a line.
 * Return:	positive number of read characters
 *		-1 if break
 */

static int ubreadline_into_buffer(const char *prompt, char *p_buf)
{
	int n = 0;				/* buffer index		*/
	int cursor;				/* cursor position in buffer */
	int shift;
	u8  c;

	p_buf[0] = '\0';

	/* print prompt */
	printf("\r%s", prompt);
	cursor = 0;

	for (;;) {
		c = serial_getchar();

		if (escape_handled(&c))
			continue;
		/*
		 * Special character handling
		 */
		switch (c) {
		case '\r':			/* Enter		*/
		case '\n':
			p_buf[n] = '\0';
			printf("\r\n");

			/* Reset history browsing. */
			history_spot = history_bottom;
			return n;

		case '\0':			/* nul			*/
			break;

		case CHAR_HOME:
			move_cursor_left(cursor);
			cursor = 0;
			break;

		case 0x03:			/* ^C - break		*/
			p_buf[0] = '\0';	/* discard input */
			return -1;

		case CHAR_EOL:
			move_cursor_right(n - cursor);
			cursor = n;
			break;

		case CHAR_RIGHT:
			if (cursor < n) {
				cursor++;
				move_cursor_right(1);
			}
			break;

		case 0x15:			/* ^U - erase line	*/
			if (cursor != n)
				move_cursor_right(n - cursor);
			while (n) {
				printf(erase_seq);
				n--;
			}
			cursor = 0;
			break;

		case 0x17:			/* ^W - erase word	*/
			erase_word(p_buf, &n, &cursor);
			break;

		case 0xb:			/* ^K  - delete to EOL */
			shift = n - cursor;
			while (n > cursor) {
				printf(" ");
				n--;
			}
			move_cursor_left(shift);
			break;

		case 0x08:			/* ^H  - backspace	*/
			backspace(p_buf, &n, &cursor);
			break;

		case CHAR_DEL:
			if (cursor < n) {
				cursor++;
				move_cursor_right(1);
				backspace(p_buf, &n, &cursor);
			}
			break;

		case CHAR_LEFT:
			if (cursor > 0) {
				cursor--;
				move_cursor_left(1);
			}
			break;

		case CHAR_UP:
		case CHAR_DOWN:
			history_case(c, p_buf, &n, &cursor);
			break;

		default:
			if ((c != '\t') && (c < ' '))
				/* No other control characters please. */
				break;
			/*
			 * Must be a normal character then
			 */
			if (n >= (MAX_CONSOLE_LINE - 2)) {
				serial_putchar('\a');
				break;
			}

			if (cursor != n) {
				shift = n - cursor;

				memmove(p_buf + cursor + 1, p_buf + cursor, shift);
				p_buf[cursor++] = c;
				n++;
				p_buf[n] = '\0';
				printf("%c%s", c, p_buf + cursor);
				move_cursor_left(shift);
				break;
			}
			if (c == '\t') {	/* expand TABs */
				/* if auto completion triggered just continue */
				p_buf[n] = '\0';
				if (!cmd_auto_complete(prompt, p_buf, &n))
					cursor = n;
				break;
			}
			p_buf[n++] = c;
			cursor++;
			/*
			 * Echo input using printf() to force an
			 * LCD flush if we are using an LCD
			 */
			p_buf[n] = '\0';
			printf("%s", p_buf + n - 1);
			break;
		}
	}
}

void console_loop(void)
{
	int len, flag, rc = 0;
	char lastcommand[128] = {0};

	/*
	 * Main Loop for Monitor Command Processing
	 */
	while (!console_done) {
		len = ubreadline_into_buffer(CONFIG_SYS_PROMPT, console_buffer);

		flag = 0;	/* assume no special flags for now */
		if (len > 0) {
			history_add(console_buffer);
			strcpy(lastcommand, console_buffer);
		} else if (len == 0)
			flag |= CMD_FLAG_REPEAT;

		if (len == -1)
			printf("<INTERRUPT>\n");
		else
			rc = run_command(lastcommand, flag);

		if (rc <= 0) {
			/* invalid command or not repeatable, forget it */
			lastcommand[0] = 0;
		}
	}
}

static int parse_line (char *line, char *argv[])
{
	int nargs = 0;

	debug_parser("parse_line: \"%s\"\n", line);
	while (nargs < CONFIG_SYS_MAXARGS) {

		/* skip any white space */
		while (isblank(*line))
			++line;

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
			debug_parser("parse_line: nargs=%d\n", nargs);
			return nargs;
		}

		argv[nargs++] = line;	/* begin of argument string	*/

		/* find end of string */
		while (*line && !isblank(*line))
			++line;

		if (*line == '\0') {	/* end of line, no more args	*/
			argv[nargs] = NULL;
			debug_parser("parse_line: nargs=%d\n", nargs);
			return nargs;
		}

		*line++ = '\0';		/* terminate current arg	 */
	}

	printf("** Too many args (max. %d) **\n", CONFIG_SYS_MAXARGS);

	debug_parser("parse_line: nargs=%d\n", nargs);
	return (nargs);
}

static void process_macros(const char *input, char *output)
{
	char c, prev;
	const char *varname_start = NULL;
	int inputcnt = strlen(input);
	int outputcnt = MAX_CONSOLE_LINE;
	int state = 0;		/* 0 = waiting for '$'  */

	/* 1 = waiting for '(' or '{' */
	/* 2 = waiting for ')' or '}' */
	/* 3 = waiting for '''  */
	char *output_start = output;

	debug_parser("[PROCESS_MACROS] INPUT len %zd: \"%s\"\n", strlen(input),
		     input);

	prev = '\0';		/* previous character   */

	while (inputcnt && outputcnt) {
		c = *input++;
		inputcnt--;

		if (state != 3) {
			/* remove one level of escape characters */
			if ((c == '\\') && (prev != '\\')) {
				if (inputcnt-- == 0)
					break;
				prev = c;
				c = *input++;
			}
		}

		switch (state) {
		case 0:	/* Waiting for (unescaped) $    */
			if ((c == '\'') && (prev != '\\')) {
				state = 3;
				break;
			}
			if ((c == '$') && (prev != '\\')) {
				state++;
			} else {
				*(output++) = c;
				outputcnt--;
			}
			break;
		case 1:	/* Waiting for (        */
			if (c == '(' || c == '{') {
				state++;
				varname_start = input;
			} else {
				state = 0;
				*(output++) = '$';
				outputcnt--;

				if (outputcnt) {
					*(output++) = c;
					outputcnt--;
				}
			}
			break;
		case 2:	/* Waiting for )        */
			if (c == ')' || c == '}') {
				int i;
				char envname[MAX_CONSOLE_LINE], *envval;
				int envcnt = input - varname_start - 1;	/* Varname # of chars */

				/* Get the varname */
				for (i = 0; i < envcnt; i++) {
					envname[i] = varname_start[i];
				}
				envname[i] = 0;

				/* Get its value */
				envval = getenv(envname);

				/* Copy into the line if it exists */
				if (envval != NULL)
					while ((*envval) && outputcnt) {
						*(output++) = *(envval++);
						outputcnt--;
					}
				/* Look for another '$' */
				state = 0;
			}
			break;
		case 3:	/* Waiting for '        */
			if ((c == '\'') && (prev != '\\')) {
				state = 0;
			} else {
				*(output++) = c;
				outputcnt--;
			}
			break;
		}
		prev = c;
	}

	if (outputcnt)
		*output = 0;
	else
		*(output - 1) = 0;

	debug_parser("[PROCESS_MACROS] OUTPUT len %zd: \"%s\"\n",
		     strlen(output_start), output_start);
}

/****************************************************************************
 * returns:
 *	1  - command executed, repeatable
 *	0  - command executed but not repeatable, interrupted commands are
 *	     always considered not repeatable
 *	-1 - not executed (unrecognized, bootd recursion or too many args)
 *           (If cmd is NULL or "" or longer than MAX_CONSOLE_LINE-1 it is
 *           considered unrecognized)
 *
 * WARNING:
 *
 * We must create a temporary copy of the command since the command we get
 * may be the result from getenv(), which returns a pointer directly to
 * the environment data, which may change magicly when the command we run
 * creates or modifies environment variables (like "bootp" does).
 */
static int builtin_run_command(const char *cmd, int flag)
{
	char cmdbuf[MAX_CONSOLE_LINE];	/* working copy of cmd		*/
	char *token;			/* start of token in cmdbuf	*/
	char *sep;			/* end of token (separator) in cmdbuf */
	char finaltoken[MAX_CONSOLE_LINE];
	char *str = cmdbuf;
	char *argv[CONFIG_SYS_MAXARGS + 1];	/* NULL terminated	*/
	int argc, inquotes;
	int repeatable = 1;
	int rc = 0;

	debug_parser("[RUN_COMMAND] cmd[%p]=\"", cmd);
	if (DEBUG_PARSER)
		/* use printf - string may be loooong */
		printf("%s\"\n", cmd ? cmd : "NULL");

	if (!cmd || !*cmd) {
		return -1;	/* empty command */
	}

	if (strlen(cmd) >= MAX_CONSOLE_LINE) {
		printf ("## Command too long!\n");
		return -1;
	}

	strcpy(cmdbuf, cmd);

	/* Process separators and check for invalid
	 * repeatable commands
	 */

	debug_parser("[PROCESS_SEPARATORS] %s\n", cmd);
	while (*str) {

		/*
		 * Find separator, or string end
		 * Allow simple escape of ';' by writing "\;"
		 */
		for (inquotes = 0, sep = str; *sep; sep++) {
			if ((*sep=='\'') &&
			    (*(sep-1) != '\\'))
				inquotes=!inquotes;

			if (!inquotes &&
			    (*sep == ';') &&	/* separator		*/
			    ( sep != str) &&	/* past string start	*/
			    (*(sep-1) != '\\'))	/* and NOT escaped	*/
				break;
		}

		/*
		 * Limit the token to data between separators
		 */
		token = str;
		if (*sep) {
			str = sep + 1;	/* start of command for next pass */
			*sep = '\0';
		}
		else
			str = sep;	/* no more commands for next pass */
		debug_parser("token: \"%s\"\n", token);

		/* find macros in this token and replace them */
		process_macros(token, finaltoken);

		/* Extract arguments */
		if ((argc = parse_line (finaltoken, argv)) == 0) {
			rc = -1;	/* no command at all */
			continue;
		}

		if (cmd_process(flag, argc, argv, &repeatable, NULL))
			rc = -1;
	}

	return rc ? rc : repeatable;
}

/*
 * Run a command using the selected parser.
 *
 * @param cmd	Command to run
 * @param flag	Execution flags (CMD_FLAG_...)
 * @return 0 on success, or != 0 on error.
 */
int run_command(const char *cmd, int flag)
{
	/*
	 * builtin_run_command can return 0 or 1 for success, so clean up
	 * its result.
	 */
	if (builtin_run_command(cmd, flag) == -1)
		return 1;

	return 0;
}

/**
 * Execute a list of command separated by ; or \n using the built-in parser.
 *
 * This function cannot take a const char * for the command, since if it
 * finds newlines in the string, it replaces them with \0.
 *
 * @param cmd	String containing list of commands
 * @param flag	Execution flags (CMD_FLAG_...)
 * @return 0 on success, or != 0 on error.
 */
static int builtin_run_command_list(char *cmd, int flag)
{
	char *line, *next;
	int rcode = 0;

	/*
	 * Break into individual lines, and execute each line; terminate on
	 * error.
	 */
	line = next = cmd;
	while (*next) {
		if (*next == '\n') {
			*next = '\0';
			/* run only non-empty commands */
			if (*line) {
				debug("** exec: \"%s\"\n", line);
				if (builtin_run_command(line, 0) < 0) {
					rcode = 1;
					break;
				}
			}
			line = next + 1;
		}
		++next;
	}
	if (rcode == 0 && *line)
		rcode = (builtin_run_command(line, 0) >= 0);

	return rcode;
}

int run_command_list(const char *cmd, int len, int flag)
{
	int need_buff = 1;
	char *buff = (char *)cmd;	/* cast away const */
	int rcode = 0;

	if (len == -1) {
		len = strlen(cmd);
		/* the built-in parser will change our string if it sees \n */
		need_buff = strchr(cmd, '\n') != NULL;
	}
	if (need_buff) {
		buff = malloc(len + 1);
		if (!buff)
			return 1;
		memcpy(buff, cmd, len);
		buff[len] = '\0';
	}
	/*
	 * This function will overwrite any \n it sees with a \0, which
	 * is why it can't work with a const char *. Here we are making
	 * using of internal knowledge of this function, to avoid always
	 * doing a malloc() which is actually required only in a case that
	 * is pretty rare.
	 */
	rcode = builtin_run_command_list(buff, flag);
	if (need_buff)
		free(buff);

	return rcode;
}

int ctrlc(void)
{
	while (serial_havechar())
		if (serial_getchar() == CHAR_CTL)
			return 1;

	return 0;
}

int read_line (const char *prompt, char *buffer)
{
	return ubreadline_into_buffer(prompt, buffer);
}
