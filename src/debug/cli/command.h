/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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

/*
 *  Definitions for Command Processor
 */
#ifndef __DEBUG_CLI_COMMAND_H__
#define __DEBUG_CLI_COMMAND_H__

/* Default to a width of 8 characters for help message command width */
#ifndef CONFIG_SYS_HELP_CMD_WIDTH
#define CONFIG_SYS_HELP_CMD_WIDTH	8
#endif

enum {
	CMD_INFO_MASK		= 0xffffff,	/* user data */
	CMD_INFO_REPEATABLE_SHIFT = 24,	/* commaand is repeatable */
	CMD_INFO_REPEATABLE	= 1U << CMD_INFO_REPEATABLE_SHIFT,
};

/*
 * Monitor Command Table
 */

struct cmd_tbl_s {
	char		*name;		/* Command Name			*/
	int		maxargs;	/* maximum number of arguments	*/
	int		info;		/* CMD_INFO_... flags		*/
					/* Implementation function	*/
	int		(*cmd)(struct cmd_tbl_s *, int, int, char * const []);
	char		*usage;		/* Usage message	(short)	*/
	char		*help;		/* Help  message	(long)	*/
	/* do auto completion on the arguments */
	int		(*complete)(int argc, char * const argv[],
				    char last_char, int maxv, char *cmdv[]);
};

typedef struct cmd_tbl_s	cmd_tbl_t;

extern char console_buffer[];
extern int read_line (const char *prompt, char *buffer);

extern int cmd_auto_complete(const char *const prompt, char *buf, int *np);
extern int cmd_get_data_size(char* arg, int default_size);

/*
 * Error codes that commands return to cmd_process(). We use the standard 0
 * and 1 for success and failure, but add one more case - failure with a
 * request to call cmd_usage(). But the cmd_process() function handles
 * CMD_RET_USAGE itself and after calling cmd_usage() it will return 1.
 * This is just a convenience for commands to avoid them having to call
 * cmd_usage() all over the place.
 */
enum command_ret_t {
	CMD_RET_SUCCESS,	/* 0 = Success */
	CMD_RET_FAILURE,	/* 1 = Failure */
	CMD_RET_USAGE = -1,	/* Failure, please report 'usage' error */
};

/**
 * Process a command with arguments. We look up the command and execute it
 * if valid. Otherwise we print a usage message.
 *
 * @param flag		Some flags normally 0 (see CMD_FLAG_.. above)
 * @param argc		Number of arguments (arg 0 must be the command text)
 * @param argv		Arguments
 * @param repeatable	This function sets this to 0 if the command is not
 *			repeatable. If the command is repeatable, the value
 *			is left unchanged.
 * @param ticks		If ticks is not null, this function set it to the
 *			number of ticks the command took to complete.
 * @return 0 if the command succeeded, 1 if it failed
 */
int cmd_process(int flag, int argc, char * const argv[],
			       int *repeatable, unsigned long *ticks);


/*
 * Command Flags:
 */
#define CMD_FLAG_REPEAT		0x0001	/* repeat last command		*/
#define CMD_FLAG_BOOTD		0x0002	/* command is from bootd	*/

# define _CMD_COMPLETE(x) x,
# define _CMD_HELP(x) x,


/*
 * These macros help declare commands. They are set up in such as way that
 * it is possible to completely remove all commands when CONFIG_CMDLINE is
 * not defined.
 *
 * Usage is:
 * U_BOOT_SUBCMD_START(name)
 *	U_BOOT_CMD_MKENT(...)		(or U_BOOT_CMD_MKENT_COMPLETE)
 *	U_BOOT_CMD_MKENT(...)
 *	...
 * U_BOOT_SUBCMD_END
 *
 * We need to ensure that a command is placed between each entry
 */
#define U_BOOT_SUBCMD_START(name)	static cmd_tbl_t name[] = {
#define U_BOOT_SUBCMD_END		};
#define U_BOOT_CMD_MKENT_LINE(_name, _maxargs, _rep, _cmd, _usage,	\
			      _help, _comp, _info)			\
	{ #_name, _maxargs, (_rep) << CMD_INFO_REPEATABLE_SHIFT | (_info), \
		_cmd, _usage, _CMD_HELP(_help) _CMD_COMPLETE(_comp) }

/* Add a comma to separate lines */
#define U_BOOT_CMD_MKENT_COMPLETE(_name, _maxargs, _rep, _cmd, _usage,	\
				  _help, _comp, _info)			\
	U_BOOT_CMD_MKENT_LINE(_name, _maxargs, _rep, _cmd, _usage,	\
				  _help, _comp, _info),

/* Here we want a semicolon after the (single) entry */
#define U_BOOT_CMD_COMPLETE(_name, _maxargs, _rep, _cmd, _usage, _help,	\
			    _comp, _info)				\
	ll_entry_declare(cmd_tbl_t, _name, cmd) =			\
		U_BOOT_CMD_MKENT_LINE(_name, _maxargs, _rep, _cmd,	\
			_usage, _help, _comp, _info);

#define U_BOOT_CMD_MKENT(_name, _maxargs, _rep, _cmd, _usage, _help)	\
	U_BOOT_CMD_MKENT_COMPLETE(_name, _maxargs, _rep, _cmd,		\
				  _usage, _help, NULL, 0)

#define U_BOOT_CMD(_name, _maxargs, _rep, _usage, _help)		\
	U_BOOT_CMD_COMPLETE(_name, _maxargs, _rep, do_##_name, _usage,	\
			    _help, NULL, 0)

#endif /* __DEBUG_CLI_COMMAND_H__ */
