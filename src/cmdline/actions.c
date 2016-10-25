/*
 * This file is part of FIRMSMITH.
 * Copyright (C) 2013 Matthias Braun <matze@braunis.de>
 */
#include "actions.h"

#include <libfirm/firm.h>
#include <stdio.h>
#include <stdlib.h>

#include "version.h"
#include <revision.h>

#define FIRMSMITH_MAJOR "1"
#define FIRMSMITH_MINOR "0"
#define FIRMSMITH_PATCHLEVEL "0"
#define FIRMSMITH_REVISION "0"

int action_version(const char *argv0)
{
	(void)argv0;
	printf("firmsmith %s.%s.%s",
	       FIRMSMITH_MAJOR, FIRMSMITH_MINOR, FIRMSMITH_PATCHLEVEL);
	if (FIRMSMITH_REVISION[0] != '\0') {
		printf("(%s)", FIRMSMITH_REVISION);
	}
	printf(" using libFirm %u.%u",
	       ir_get_version_major(), ir_get_version_minor());

	const char *revision = ir_get_version_revision();
	if (revision[0] != 0) {
		printf("(%s)", revision);
	}
	putchar('\n');
	return EXIT_SUCCESS;
}

int action_version_short(const char *argv0)
{
	(void)argv0;
	printf("%s.%s.%s\n",
	       FIRMSMITH_MAJOR, FIRMSMITH_MINOR, FIRMSMITH_PATCHLEVEL);
	return EXIT_SUCCESS;
}
