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
	char const *const option = &s->argv[s->i][1];
	if (streq(option, arg)) {
		if (s->i + 1 < s->argc)
			return s->argv[++s->i];
		fprintf(stderr, "expected argument after '-%s'\n", arg);
		s->argument_errors = true;
	}
	return NULL;
}

static const char *equals_arg(const char *prefix, options_state_t *s)
{
	char const *const option = &s->argv[s->i][1];
	char const *const equals = strstart(option, prefix);
	if (equals) {
		if (equals[0] == '=') {
			char const *const arg = equals + 1;
			if (arg[0] != '\0')
				return arg;
			fprintf(stderr, "expected argument after '-%s'\n", option);
			s->argument_errors = true;
		} else if (equals[0] == '\0') {
			fprintf(stderr, "expected '=' and argument after '-%s'\n", option);
			s->argument_errors = true;
		}
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

bool options_parse_cf(options_state_t *s)
{
	const char *full_option = s->argv[s->i];
	if (full_option[0] != '-')
		return false;

	const char *arg;
	if (simple_arg("-help", s)) {
		s->action = action_help;
	} else if ((arg = spaced_arg("-graphsize", s)) != NULL) {
		int n = atoi(arg);
		set_cfg_size(n);
	}else if ((arg = spaced_arg("-blocksize", s)) != NULL) {
		int n = atoi(arg);
		set_blocksize(n);
	} else {
		return false;
	}
	return true;
}
