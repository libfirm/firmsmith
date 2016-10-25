/*
 * This file is part of cparser.
 * Copyright (C) 2013 Matthias Braun <matze@braunis.de>
 */
#ifndef OPTIONS_H
#define OPTIONS_H

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

bool options_parse_cf(options_state_t *s);

bool action_print_help(const char *argv0);
void setup_target_machine(void);

char const *spaced_arg(char const *arg, options_state_t *s);
bool simple_arg(const char *arg, options_state_t *s);

#endif
