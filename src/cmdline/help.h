/*
 * This file is part of cparser.
 * Copyright (C) 2012 Matthias Braun <matze@braunis.de>
 */
#ifndef HELP_H
#define HELP_H

typedef enum {
	HELP_NONE          = 0,
	HELP_BASIC         = 1 << 0,
	HELP_ALL           = -1
} help_sections_t;

void help_usage(const char *argv0);

int action_help(const char *argv0);

extern help_sections_t help;

#endif
