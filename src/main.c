#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libfirm/firm.h>
#include <time.h>
#include <sys/time.h>

#include "lib/statistics.h"
#include "lib/optimizations.h"
#include "lib/convert.h"
#include "lib/resolve.h"
#include "cmdline/options.h"
#include "cmdline/help.h"
#include <assert.h>

#define LINK_COMMAND          "grep -vE '(\\.type|\\.size)' a.s > a.S && cc -m32 a.S"

time_t seed = 0;

int nostats = 0;
char strid[256] = "main";

static int action_run(const char *argv0) {
	(void)argv0;
	printf("Used seed: %ld\n\n", seed);

	// Create random function
	func_t* func = new_random_func();
	// Construct corresponding ir node tree
	convert_func(func);
	//cfg_print(func->cfg);
	resolve_cfg_temporaries(func->cfg);
	resolve_mem_graph(func->cfg);
	finalize_convert(func->cfg);

	irg_assert_verify(get_current_ir_graph());
	// Dump ir file
	char ir_file_name[256];
	snprintf(ir_file_name, sizeof ir_file_name, "%s.ir", strid);
	FILE *irout = fopen(ir_file_name, "w");
	ir_export_file(irout);
	fclose(irout);

	dump_all_ir_graphs(strid);
	
	/*
	(void)get_optimized_graph(get_current_ir_graph());
	assert(irg_verify(get_current_ir_graph()) && "valid graph");


	// Code generation
	FILE *out = fopen("a.s", "w");
	if(out == NULL) {
		fprintf(stderr, "couldn't open a.s for writing: %s\n", strerror(errno));
		return 1;
	}
	be_main(out, "");
	fclose(out);


	// Stats
	if (!nostats) {
		print_cfg_stats(func->cfg);
		print_op_stats(func->cfg);
	}

	// finish libfirm
	ir_finish();

	system(LINK_COMMAND);
	*/
	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	options_state_t state;
	memset(&state, 0, sizeof(state));
	state.argc   = argc;
	state.argv   = argv;
	state.action = action_run;

	// initialize libFirm
	ir_init();
	set_opt_optimize(0);

	// Set seed
	struct timeval tval;
	gettimeofday(&tval, NULL);

	time(&seed);
	seed ^= tval.tv_usec;
	//t=1475511347l;
	//t=1476053236l;

	/* parse rest of options */
	const char *arg;
	for (state.i = 1; state.i < state.argc; ++state.i) {
		if (options_parse_cf(&state)) {
			continue;
		} else if ((arg = spaced_arg("-seed", &state)) != NULL) {
			seed = atol(arg);
		} else if (simple_arg("-nostats", &state)) {
			nostats = 1;	
		} else if ((arg = spaced_arg("-strid", &state)) != NULL) {
			strncpy(strid, arg, sizeof strid);
		} else {
			fprintf(stderr, "unknown argument '%s'\n", argv[state.i]);
			state.argument_errors = true;
		}
	}
	if (state.argument_errors) {
		action_help(argv[0]);
		return EXIT_FAILURE;
	}
	srand(seed);

	assert(state.action != NULL);
	int ret = state.action(argv[0]);

	return ret;
}
