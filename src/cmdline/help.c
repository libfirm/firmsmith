/*
 * This file is part of cparser.
 * Copyright (C) 2012 Matthias Braun <matze@braunis.de>
 */
#include "help.h"

#include <assert.h>
#include <libfirm/be.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void help_f_yesno(char *option, const char *explanation)
{
	assert(option[0] == '-' && option[1] == 'f');
	int yesno_option_len     = strlen(option) + 4;
	int yesno_explanation_len = strlen(explanation) + 10;
	char *yesno_option      = malloc(yesno_option_len);
	char *yesno_explanation = malloc(yesno_explanation_len);

	snprintf(yesno_explanation, yesno_explanation_len, "Enable %s", explanation);
	help_simple(option, yesno_explanation);

	snprintf(yesno_option, yesno_option_len, "-fno-%s", option + 2);
	snprintf(yesno_explanation, yesno_explanation_len, "Disable %s", explanation);	
	help_simple(yesno_option, yesno_explanation);
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
	help_simple("--help",               "Display this information");
	help_simple("--version",            "Display compiler version");
	help_spaced("--seed", "n", 		    "Set seed for random graph generation");
	help_spaced("--strid", "id",	    "Set identifier used in output file generation");
	help_f_yesno("-fstats", 		    "printing of generated graph statistics");
	help_f_yesno("-ffunc-cycles", 	    "generation of cyclic function call graphs");
	help_f_yesno("-ffunc-calls", 	    "generation of function calls");
	help_f_yesno("-floops", 	    	"generation of loops in control flow graph");
	help_f_yesno("-fmemory", 	    	"generation memory operations");
	help_spaced("--nfuncs", "n",		"Number of generated functions");
	help_spaced("--func-maxcalls", "n",	"Set limit for number of functions calls inside function");
	help_spaced("--cfg-size", "n",		"Set number of generated blocks in control flow graph");
	help_spaced("--cfb-size", "id",		"Set number of generated nodes in control flow block");

}

int action_help(const char *argv0)
{
	print_help_basic(argv0);
	return EXIT_SUCCESS;
}
