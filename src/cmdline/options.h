/*
 * This file is part of cparser.
 * Copyright (C) 2013 Matthias Braun <matze@braunis.de>
 */
#ifndef OPTIONS_H
#define OPTIONS_H

#include "parameters.h"
#include <stdbool.h>

typedef int (*action_func)(const char *argv0);

typedef struct options_state_t {
	int         argc;
	char      **argv;
	int         i;
	bool        argument_errors;
	bool        had_inputs;
	action_func action;
} options_state_t;

bool options_parse(options_state_t *s);

char const *spaced_arg(char const *arg, options_state_t *s);
bool simple_arg(const char *arg, options_state_t *s);

#endif
