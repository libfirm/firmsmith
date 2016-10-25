/*
 * This file is part of cparser.
 * Copyright (C) 2012 Matthias Braun <matze@braunis.de>
 */
#include "help.h"

#include <assert.h>
#include <libfirm/be.h>
#include <stdio.h>
#include <stdlib.h>

help_sections_t help;

/** Print help for a simpel_arg() option. */
static void help_simple(const char *option, const char *explanation)
{
	printf("\t%-15s  %s\n", option, explanation);
}

/** Print help for a prefix_arg() option. */
static void help_prefix(const char *option, const char *optname,
                        const char *explanation)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "%s %s", option, optname);
	help_simple(buf, explanation);
}

/** Print help for an accept_prefix() option. */
static void help_aprefix(const char *prefix, const char *optname,
                         const char *explanation)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "%s%s", prefix, optname);
	help_simple(buf, explanation);
}

/** Print help for a spaced_arg() option. */
static void help_spaced(const char *option, const char *optname,
                        const char *explanation)
{
	help_prefix(option, optname, explanation);
}

/** Print help for an equals_arg() option. */
static void help_equals(const char *option, const char *optname,
                        const char *explanation)
{
	char buf[128];
	snprintf(buf, sizeof(buf), "%s=%s", option, optname);
	help_simple(buf, explanation);
}

static void help_f_yesno(const char *option, const char *explanation)
{
	help_simple(option, explanation);
}

static void put_choice(const char *choice, const char *explanation)
{
	printf("\t    %-11s  %s\n", choice, explanation);
}

void help_usage(const char *argv0)
{
	fprintf(stderr, "Usage %s [options]\n", argv0);
}

static void print_help_basic(const char *argv0)
{
	help_usage(argv0);
	puts("");
	help_simple("--help",                   "Display this information");
	help_simple("--version",                "Display compiler version");
	help_spaced("--graphsize", "n", 		"Set number of control flow blocks");
	help_spaced("--blocksize", "n", 		"Set average number of nodes inside block");
	help_spaced("--seed", "n", 				"Set seed for random graph generation");
	help_simple("--nostats",				"Do not output stats");
}

int action_help(const char *argv0)
{
	print_help_basic(argv0);
	return EXIT_SUCCESS;
}
