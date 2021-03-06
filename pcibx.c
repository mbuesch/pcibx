/*

  Catalyst PCIBX32 PCI Extender control utility

  Copyright (c) 2006-2009 Michael Buesch <mb@bu3sch.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
  Boston, MA 02110-1301, USA.

*/

#include "pcibx.h"
#include "pcibx_device.h"

#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>


struct cmdline_args cmdargs;
struct timeval starttime;


/* Subtract the `struct timeval' values X and Y,
 * storing the result in RESULT.
 * Return 1 if the difference is negative, otherwise 0.
 *
 * Taken from the glibc docs, so should be
 * Copyright (c) Free Software Foundation
 */
static int timeval_subtract(struct timeval *result,
			    struct timeval *x,
			    struct timeval *y)
{
	int nsec;

	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec) {
		nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}
	/* Compute the time remaining to wait.
	 * tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}

#define print_data(description, format, units, ...) do { \
	struct timeval tmp, tv;						\
	int err;							\
									\
	if (cmdargs.verbose >= 1) {					\
		err = gettimeofday(&tmp, NULL);				\
		if (err) {						\
			prerror("gettimeofday() failure!\n");		\
			break;						\
		}							\
		err = timeval_subtract(&tv, &tmp, &starttime);		\
		if (err) {						\
			prerror("timeval_subtract() went negative!\n");	\
			break;						\
		}							\
		prinfo("%ld.%06ld " format "  # ",			\
		       tv.tv_sec, tv.tv_usec,				\
		       __VA_ARGS__);					\
	}								\
	prinfo(description ": " format " " units "\n", __VA_ARGS__);	\
} while (0)

static int send_commands(struct pcibx_device *dev)
{
	struct pcibx_command *cmd;
	uint8_t v;
	float f;
	int i;

	for (i = 0; i < cmdargs.nr_commands; i++) {
		cmd = &(cmdargs.commands[i]);

		switch (cmd->id) {
		case CMD_GLOB:
			pcibx_cmd_global_pwr(dev, cmd->u.boolean);
			break;
		case CMD_UUT:
			pcibx_cmd_uut_pwr(dev, cmd->u.boolean);
			break;
		case CMD_PRINTBOARDID:
			v = pcibx_cmd_getboardid(dev);
			print_data("Board ID", "0x%02X", "", v);
			break;
		case CMD_PRINTFIRMREV:
			v = pcibx_cmd_getfirmrev(dev);
			print_data("Firmware revision", "0x%02X", "", v);
			break;
		case CMD_PRINTSTATUS:
			v = pcibx_cmd_getstatus(dev);
			print_data("Board status", "%s;  %s;  %s;  %s;  %s", "",
			       (v & PCIBX_STATUS_RSTDEASS) ? "RST# de-asserted"
							   : "RST# asserted",
			       (v & PCIBX_STATUS_64BIT) ? "64-bit operation established"
							: "No 64-bit handshake detected",
			       (v & PCIBX_STATUS_32BIT) ? "32-bit operation established"
							: "No 32-bit handshake detected",
			       (v & PCIBX_STATUS_MHZ) ? "66 Mhz enabled slot"
						      : "33 Mhz enabled slot",
			       (v & PCIBX_STATUS_DUTASS) ? "DUT asserted"
							 : "DUT not fully asserted");
			break;
		case CMD_CLEARBITSTAT:
			pcibx_cmd_clearbitstat(dev);
			break;
		case CMD_AUX5:
			pcibx_cmd_aux5(dev, cmd->u.boolean);
			break;
		case CMD_AUX33:
			pcibx_cmd_aux33(dev, cmd->u.boolean);
			break;
		case CMD_MEASUREFREQ:
			f = pcibx_cmd_sysfreq(dev);
			print_data("Measured system frequency", "%f", "Mhz", f);
			break;
		case CMD_MEASUREV25REF:
			f = pcibx_cmd_measure(dev, MEASURE_V25REF);
			print_data("Measured +2.5V Reference", "%f", "Volt", f);
			break;
		case CMD_MEASUREV12UUT:
			f = pcibx_cmd_measure(dev, MEASURE_V12UUT);
			print_data("Measured +12V UUT", "%f", "Volt", f);
			break;
		case CMD_MEASUREV5UUT:
			f = pcibx_cmd_measure(dev, MEASURE_V5UUT);
			print_data("Measured +5V UUT", "%f", "Volt", f);
			break;
		case CMD_MEASUREV33UUT:
			f = pcibx_cmd_measure(dev, MEASURE_V33UUT);
			print_data("Measured +33V UUT", "%f", "Volt", f);
			break;
		case CMD_MEASUREV5AUX:
			f = pcibx_cmd_measure(dev, MEASURE_V5AUX);
			print_data("Measured +5V AUX", "%f", "Volt", f);
			break;
		case CMD_MEASUREA5:
			f = pcibx_cmd_measure(dev, MEASURE_A5);
			print_data("Measured +5V Current", "%f", "Ampere", f);
			break;
		case CMD_MEASUREA12:
			f = pcibx_cmd_measure(dev, MEASURE_A12);
			print_data("Measured +12V Current", "%f", "Ampere", f);
			break;
		case CMD_MEASUREA33:
			f = pcibx_cmd_measure(dev, MEASURE_A33);
			print_data("Measured +3.3V Current", "%f", "Ampere", f);
			break;
		case CMD_FASTRAMP:
			pcibx_cmd_ramp(dev, cmd->u.boolean);
			break;
		case CMD_RST:
			pcibx_cmd_rst(dev, cmd->u.d);
			break;
		case CMD_RSTDEFAULT:
			pcibx_cmd_rstdefault(dev);
			break;
		case CMD_GETPME:
			v = pcibx_cmd_getpme(dev);
			print_data("PME# status", "0x%02X", "", v);
			break;
		default:
			internal_error("invalid command");
			return -1;
		}
	}
	if (cmdargs.verbose >= 2)
		prinfo("All commands sent.\n");

	return 0;
}

static int request_priority(void)
{
	struct sched_param param;
	int err;

	param.sched_priority = sched_get_priority_max(cmdargs.sched);
	err = sched_setscheduler(0, cmdargs.sched, &param);
	if (err) {
		prerror("Could not set scheduling policy (%s).\n",
			strerror(errno));
	}

	return err;
}

static void print_banner(void)
{
	prinfo("Catalyst PCIBX32 PCI Extender control utility version " VERSION "\n"
	       "\n"
	       "Copyright 2006-2009 Michael Buesch <mb@bu3sch.de>\n"
	       "Licensed under the GNU General Public License v2+\n"
	       "\n");
}

static void print_usage(int argc, char **argv)
{
	prinfo("Usage: %s [OPTION]\n", argv[0]);
	prinfo("  -V|--verbose LEVEL    Verbosity level 0-2 (default: 0)\n");
	prinfo("  -v|--version          Print version\n");
	prinfo("  -h|--help             Print this help\n");
	prinfo("\n");
	prinfo("  -p|--port /dev/parportX  Parport device (Default: /dev/parport0)\n");
	prinfo("  -P|--pci1 BOOL        If true, PCI_1 (default), otherwise PCI_2. (See JP15)\n");
	prinfo("  -s|--sched POLICY     Scheduling policy (normal, fifo, rr)\n");
	prinfo("  -n|--nrcycle COUNT    Cycle COUNT times. 0 = infinite (default: 1)\n");
	prinfo("  -d|--delay DELAY      DELAY msecs after each cycle. Default 0\n");
	prinfo("\n");
	prinfo("Device commands\n");
	prinfo("  --cmd-glob ON/OFF     Turn Global power ON/OFF (does not turn ON UUT Voltages)\n");
	prinfo("  --cmd-uut ON/OFF      Turn UUT Voltages ON/OFF (also turns Global power ON)\n");
	prinfo("  --cmd-printboardid    Print the Board ID\n");
	prinfo("  --cmd-printfirmrev    Print the Firmware revision\n");
	prinfo("  --cmd-printstatus     Print the Board Status Bits\n");
	prinfo("  --cmd-clearbitstat    Clear 32/64 bit status\n");
	prinfo("  --cmd-aux5 ON/OFF     Turn +5V Aux ON or OFF\n");
	prinfo("  --cmd-aux33 ON/OFF    Turn +3.3V Aux ON or OFF\n");
	prinfo("  --cmd-measurefreq     Measure system frequency\n");
	prinfo("  --cmd-measurev25ref   Measure +2.5V Reference\n");
	prinfo("  --cmd-measurev12uut   Measure +12V UUT\n");
	prinfo("  --cmd-measurev5uut    Measure +5V UUT\n");
	prinfo("  --cmd-measurev33uut   Measure +3.3V UUT\n");
	prinfo("  --cmd-measurev5aux    Measure +5V AUX\n");
	prinfo("  --cmd-measurea5       Measure +5V Current\n");
	prinfo("  --cmd-measurea12      Measure +12V Current\n");
	prinfo("  --cmd-measurea33      Measure +3.3V Current\n");
	prinfo("  --cmd-fastramp ON/OFF Select slow/fast +5V ramp\n");
	prinfo("  --cmd-rst 0.150       Set RST# (reset) delay (in seconds)\n");
	prinfo("  --cmd-rstdefault      Set RST# to default (150msec)\n");
}

#define ARG_MATCH		0
#define ARG_NOMATCH		1
#define ARG_ERROR		-1

static int do_cmp_arg(char **argv, int *pos,
		      const char *template,
		      int allow_merged,
		      char **param)
{
	char *arg;
	char *next_arg;
	size_t arg_len, template_len;

	arg = argv[*pos];
	next_arg = argv[*pos + 1];
	arg_len = strlen(arg);
	template_len = strlen(template);

	if (param) {
		/* Maybe we have a merged parameter here.
		 * A merged parameter is "-pfoobar" for example.
		 */
		if (allow_merged && arg_len > template_len) {
			if (memcmp(arg, template, template_len) == 0) {
				*param = arg + template_len;
				return ARG_MATCH;
			}
			return ARG_NOMATCH;
		} else if (arg_len != template_len)
			return ARG_NOMATCH;
		*param = next_arg;
	}
	if (strcmp(arg, template) == 0) {
		if (param) {
			/* Skip the parameter on the next iteration. */
			(*pos)++;
			if (*param == 0) {
				prerror("%s needs a parameter\n", arg);
				return ARG_ERROR;
			}
		}
		return ARG_MATCH;
	}

	return ARG_NOMATCH;
}

/* Simple and lean command line argument parsing. */
static int cmp_arg(char **argv, int *pos,
		   const char *long_template,
		   const char *short_template,
		   char **param)
{
	int err;

	if (long_template) {
		err = do_cmp_arg(argv, pos, long_template, 0, param);
		if (err == ARG_MATCH || err == ARG_ERROR)
			return err;
	}
	err = ARG_NOMATCH;
	if (short_template)
		err = do_cmp_arg(argv, pos, short_template, 1, param);
	return err;
}
#define arg_match(argv, i, tlong, tshort, param) \
	({						\
		int res = cmp_arg((argv), (i), (tlong),	\
				  (tshort), (param));	\
		if ((res) == ARG_ERROR)			\
	 		goto error;			\
		((res) == ARG_MATCH);			\
	})

#if 0
static int parse_hexval(const char *str,
			uint32_t *value,
			const char *param)
{
	uint32_t v;

	if (strncmp(str, "0x", 2) != 0)
		goto error;
	str += 2;
	errno = 0;
	v = strtoul(str, NULL, 16);
	if (errno)
		goto error;
	*value = v;

	return 0;
error:
	if (param) {
		prerror("%s value parsing error. Format: 0xFFFFFFFF\n",
			param);
	}
	return -1;
}
#endif

static int parse_bool(const char *str,
		      const char *param)
{
	if (strcmp(str, "1") == 0)
		return 1;
	if (strcmp(str, "0") == 0)
		return 0;
	if (strcasecmp(str, "true") == 0)
		return 1;
	if (strcasecmp(str, "false") == 0)
		return 0;
	if (strcasecmp(str, "yes") == 0)
		return 1;
	if (strcasecmp(str, "no") == 0)
		return 0;
	if (strcasecmp(str, "on") == 0)
		return 1;
	if (strcasecmp(str, "off") == 0)
		return 0;

	if (param) {
		prerror("%s boolean parsing error. Format: BOOL\n",
			param);
	}

	return -1;
}

static int parse_double(const char *str,
			double *value,
			const char *param)
{
	double v;

	errno = 0;
	v = strtod(str, NULL);
	if (errno)
		goto error;
	*value = v;

	return 0;
error:
	if (param) {
		prerror("%s value parsing error. Format: 0.00123\n",
			param);
	}
	return -1;
}

static int parse_int(const char *str,
		     int *value,
		     const char *param)
{
	int v;

	errno = 0;
	v = strtol(str, NULL, 10);
	if (errno)
		goto error;
	*value = v;

	return 0;
error:
	if (param) {
		prerror("%s value parsing error. Format: 1234\n",
			param);
	}
	return -1;
}

static int add_command(enum command_id cmd)
{
	if (cmdargs.nr_commands == MAX_COMMAND) {
		prerror("Maximum number of commands exceed.\n");
		return -1;
	}
	cmdargs.commands[cmdargs.nr_commands++].id = cmd;

	return 0;
}

static int add_boolcommand(enum command_id cmd,
			   const char *str,
			   const char *param)
{
	int boolean;

	if (cmdargs.nr_commands == MAX_COMMAND) {
		prerror("Maximum number of commands exceed.\n");
		return -1;
	}

	boolean = parse_bool(str, param);
	if (boolean < 0)
		return -1;
	cmdargs.commands[cmdargs.nr_commands].id = cmd;
	cmdargs.commands[cmdargs.nr_commands].u.boolean = !!boolean;
	cmdargs.nr_commands++;

	return 0;
}

static int add_doublecommand(enum command_id cmd,
			     const char *str,
			     const char *param)
{
	int err;
	double value;

	if (cmdargs.nr_commands == MAX_COMMAND) {
		prerror("Maximum number of commands exceed.\n");
		return -1;
	}

	err = parse_double(str, &value, param);
	if (err)
		return err;
	cmdargs.commands[cmdargs.nr_commands].id = cmd;
	cmdargs.commands[cmdargs.nr_commands].u.d = value;
	cmdargs.nr_commands++;

	return 0;
}

static int parse_args(int argc, char **argv)
{
	int i, err;
	char *param;

	cmdargs.port = "/dev/parport0";
	cmdargs.is_PCI_1 = 1;
	cmdargs.sched = SCHED_OTHER;
	cmdargs.cycle_delay = 0;
	cmdargs.nrcycle = 1;

	for (i = 1; i < argc; i++) {
		if (arg_match(argv, &i, "--version", "-v", 0)) {
			print_banner();
			return 1;
		} else if (arg_match(argv, &i, "--help", "-h", 0)) {
			print_banner();
			print_usage(argc, argv);
			return 1;
		} else if (arg_match(argv, &i, "--verbose", "-V", &param)) {
			err = parse_int(param, &cmdargs.verbose, "--verbose");
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--port", "-p", &param)) {
			cmdargs.port = param;
		} else if (arg_match(argv, &i, "--pci1", "-P", &param)) {
			err = parse_bool(param, "--pci1");
			if (err < 0)
				goto error;
			cmdargs.is_PCI_1 = !!err;
		} else if (arg_match(argv, &i, "--sched", "-s", &param)) {
			if (strcasecmp(param, "normal") == 0)
				cmdargs.sched = SCHED_OTHER;
			else if (strcasecmp(param, "fifo") == 0)
				cmdargs.sched = SCHED_FIFO;
			else if (strcasecmp(param, "rr") == 0)
				cmdargs.sched = SCHED_RR;
			else {
				prerror("Invalid parameter to --sched\n");
				goto error;
			}
		} else if (arg_match(argv, &i, "--delay", "-d", &param)) {
			err = parse_int(param, &cmdargs.cycle_delay, "--delay");
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--nrcycle", "-n", &param)) {
			err = parse_int(param, &cmdargs.nrcycle, "--nrcycle");
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-glob", 0, &param)) {
			err = add_boolcommand(CMD_GLOB, param, "--cmd-glob");
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-uut", 0, &param)) {
			err = add_boolcommand(CMD_UUT, param, "--cmd-uut");
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-printboardid", 0, 0)) {
			err = add_command(CMD_PRINTBOARDID);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-printfirmrev", 0, 0)) {
			err = add_command(CMD_PRINTFIRMREV);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-printstatus", 0, 0)) {
			err = add_command(CMD_PRINTSTATUS);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-clearbitstat", 0, 0)) {
			err = add_command(CMD_CLEARBITSTAT);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-aux5", 0, &param)) {
			err = add_boolcommand(CMD_AUX5, param, "--cmd-aux5");
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-aux33", 0, &param)) {
			err = add_boolcommand(CMD_AUX33, param, "--cmd-aux33");
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-measurefreq", 0, 0)) {
			err = add_command(CMD_MEASUREFREQ);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-measurev25ref", 0, 0)) {
			err = add_command(CMD_MEASUREV25REF);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-measurev12uut", 0, 0)) {
			err = add_command(CMD_MEASUREV12UUT);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-measurev5uut", 0, 0)) {
			err = add_command(CMD_MEASUREV5UUT);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-measurev33uut", 0, 0)) {
			err = add_command(CMD_MEASUREV33UUT);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-measurev5aux", 0, 0)) {
			err = add_command(CMD_MEASUREV5AUX);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-measurea5", 0, 0)) {
			err = add_command(CMD_MEASUREA5);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-measurea12", 0, 0)) {
			err = add_command(CMD_MEASUREA12);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-measurea33", 0, 0)) {
			err = add_command(CMD_MEASUREA33);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-fastramp", 0, &param)) {
			err = add_boolcommand(CMD_FASTRAMP, param, "--cmd-fastramp");
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-rst", 0, &param)) {
			err = add_doublecommand(CMD_RST, param, "--cmd-rst");
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-rstdefault", 0, 0)) {
			err = add_command(CMD_RSTDEFAULT);
			if (err)
				goto error;
		} else if (arg_match(argv, &i, "--cmd-getpme", 0, 0)) {
			err = add_command(CMD_GETPME);
			if (err)
				goto error;
		} else {
			prerror("Unrecognized argument: %s\n", argv[i]);
			goto error;
		}
	}
	if (cmdargs.nr_commands == 0) {
		prerror("No device commands specified.\n\n");
		print_usage(argc, argv);
		goto error;
	}
	return 0;

error:
	return -1;
}

static void signal_handler(int sig)
{
	prinfo("Signal %d received. Terminating.\n", sig);
	exit(1);
}

static int setup_sighandler(void)
{
	int err;
	struct sigaction sa;

	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	err = sigaction(SIGINT, &sa, NULL);
	err |= sigaction(SIGTERM, &sa, NULL);
	if (err)
		prerror("sigaction setup failed.\n");

	return err;
}

int main(int argc, char **argv)
{
	struct pcibx_device dev;
	int err;
	int nrcycle;

	err = setup_sighandler();
	if (err)
		goto out;
	err = parse_args(argc, argv);
	if (err == 1)
		return 0;
	else if (err != 0)
		goto out;

	err = request_priority();
	if (err)
		goto out;

	err = pcibx_device_init(&dev, cmdargs.port, cmdargs.is_PCI_1);
	if (err)
		goto out;
	gettimeofday(&starttime, NULL);
	nrcycle = cmdargs.nrcycle;
	if (nrcycle == 0)
		nrcycle = -1;
	while (1) {
		err = send_commands(&dev);
		if (err)
			goto out_exit_dev;
		if (nrcycle > 0)
			nrcycle--;
		if (nrcycle == 0)
			break;
		if (cmdargs.cycle_delay)
			msleep(cmdargs.cycle_delay);
	}

out_exit_dev:
	pcibx_device_exit(&dev);
out:
	return err ? 1 : 0;
}
