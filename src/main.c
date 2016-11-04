#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libfirm/firm.h>
#include <time.h>
#include <sys/time.h>

#include "cmdline/parameters.h"
#include "lib/prog.h"
#include "lib/statistics.h"
#include "lib/optimizations.h"
#include "lib/convert.h"
#include "lib/resolve.h"
#include "cmdline/options.h"
#include "cmdline/help.h"
#include <assert.h>

#define LINK_COMMAND          "grep -vE '(\\.type|\\.size)' a.s > a.S && cc -m32 a.S"

int nostats = 0;

static int action_run(const char *argv0) {
	(void)argv0;
	// Create random function
	prog_t* prog = new_random_prog();
	// Construct corresponding ir node tree
	convert_prog(prog);
	//cfg_print(func->cfg);
	resolve_prog(prog);
	finalize_convert(prog);

	irg_assert_verify(get_current_ir_graph());
	// Dump ir file
	char ir_file_name[256];
	snprintf(ir_file_name, sizeof ir_file_name, "%s.ir", fs_params.prog.strid);
	FILE *irout = fopen(ir_file_name, "w");
	ir_export_file(irout);
	fclose(irout);

	dump_all_ir_graphs(fs_params.prog.strid);
	
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

	time((time_t*)&fs_params.prog.seed);
	fs_params.prog.seed ^= (int)tval.tv_usec;
	//t=1475511347l;
	//t=1476053236l;

	/* parse rest of options */
	for (state.i = 1; state.i < state.argc; ++state.i) {
		if (options_parse(&state)) {
			continue;
		} else {
			fprintf(stderr, "unknown argument '%s'\n", argv[state.i]);
			state.argument_errors = true;
		}
	}
	if (state.argument_errors) {
		action_help(argv[0]);
		return EXIT_FAILURE;
	}
	srand(fs_params.prog.seed);

	assert(state.action != NULL);
	int ret = state.action(argv[0]);

	return ret;
}
