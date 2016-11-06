/*
 * This file is part of cparser.
 * Copyright (C) 2013 Matthias Braun <matze@braunis.de>
 */
#include "options.h"
#include "help.h"
#include <assert.h>
#include <string.h>
#include "strutil.h"
#include <stdlib.h>

#include "../lib/func.h"
#include "actions.h"
#include "../lib/resolve.h"

bool simple_arg(const char *arg, options_state_t *s)
{
	if (s->argv[s->i][0] != '-') {
		return false;
	}
	const char *option = &s->argv[s->i][1];
	return streq(option, arg);
}

static const char *prefix_arg(const char *prefix, options_state_t *s)
{
	const char *option = &s->argv[s->i][1];
	if (!strstart(option, prefix))
		return NULL;

	const size_t prefix_len = strlen(prefix);
	const char  *def        = &option[prefix_len];
	if (def[0] != '\0')
		return def;

	if (s->i+1 >= s->argc) {
		fprintf(stderr, "expected argument after '-%s'\n", prefix);
		s->argument_errors = true;
		return NULL;
	}
	return s->argv[++s->i];
}

char const *spaced_arg(char const *const arg, options_state_t *const s)
{
	char const *const option = &s->argv[s->i][2];
	if (streq(option, arg)) {
		if (s->i + 1 < s->argc)
			return s->argv[++s->i];
		fprintf(stderr, "expected argument after '-%s'\n", arg);
		s->argument_errors = true;
	}
	return NULL;
}

static const char *f_no_arg(bool *truth_value, options_state_t *s)
{
	const char *option = &s->argv[s->i][1];
	if (option[0] != 'f')
		return NULL;
	++option;

	if (option[0] == 'n' && option[1] == 'o' && option[2] == '-') {
		*truth_value = false;
		option += 3;
	} else {
		*truth_value = true;
	}
	return option;
}

static bool f_yesno_arg(char const *const arg, options_state_t const *const s)
{
	const char *option = s->argv[s->i];
	assert(arg[0] == '-' && arg[1] == 'f');
	assert(option[0] == '-' && option[1] == 'f');
	if (option[2] == 'n' && option[3] == 'o' && option[4] == '-')
		option += 3;
	return streq(&arg[2], &option[2]);
}

static bool accept_prefix(options_state_t *const s, char const *const prefix,
                          bool expect_arg, char const **const arg)
{
	const char *option = s->argv[s->i];
	assert(option[0] == '-');
	assert(prefix[0] == '-');
	if (!strstart(&option[1], &prefix[1]))
		return false;

	const size_t prefix_len = strlen(prefix);
	*arg = option + prefix_len;
	if (expect_arg && (*arg)[0] == '\0') {
		fprintf(stderr, "expected argument after '-%s'\n", prefix);
		s->argument_errors = true;
		return false;
	}
	return true;
}


bool options_parse(options_state_t *s)
{
	const char *full_option = s->argv[s->i];
	if (full_option[0] != '-')
		return false;

	bool truth_value;
	if (f_no_arg(&truth_value, s) != NULL) {
		if (f_yesno_arg("-fstats", s)) {
			fs_params.prog.has_stats = true;
		} else if (f_yesno_arg("-ffunc-cycles", s)) {
			fs_params.prog.has_cycles = truth_value;
		} else if (f_yesno_arg("-ffunc-calls", s)) {
			fs_params.cfb.has_func_calls = truth_value;
		} else if (f_yesno_arg("-floops", s)) {
			fs_params.cfg.has_loops = truth_value;;	
		} else if (f_yesno_arg("-fmemory", s)) {
			fs_params.cfb.has_memory_ops = truth_value;
		} else {
			return NULL;
		}
		return true;
	}

	const char *arg;

	if ((arg = spaced_arg("seed", s)) != NULL) {
		fs_params.prog.seed = atoi(arg);
	} else if ((arg = spaced_arg("strid", s)) != NULL) {
		fs_params.prog.strid = arg;
	} else if ((arg = spaced_arg("nfuncs", s)) != NULL) {
		fs_params.prog.n_funcs = atoi(arg);
	} else if ((arg = spaced_arg("func-maxcalls", s)) != NULL) {
		fs_params.func.max_calls = atoi(arg);
	} else if ((arg = spaced_arg("cfg-size", s)) != NULL) {
		fs_params.cfg.n_blocks = atoi(arg);
	} else if ((arg = spaced_arg("cfb-size", s)) != NULL) {
		fs_params.cfb.n_nodes = atoi(arg);
	} else {
		return false;
	}
	return true;

}
