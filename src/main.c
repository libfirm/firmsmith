#include <sys/time.h>
#include <time.h>

#include "libfirm/firm.h"
#include "lib/resolve.h"
#include "lib/types.h"
#include "lib/convert.h"
#include "cmdline/options.h"
#include "cmdline/help.h"

#define LINK_COMMAND          "grep -vE '(\\.type|\\.size)' a.s > a.S && cc -m32 a.S"

int nostats = 0;

static void initialize_firmsmith() {
	//gen_firm_init();
	//firm_option("no-opt");
	//s et_optimize(0);
	ir_init();
	set_optimize(0);
	//machine_triple_t *machine = firm_get_host_machine();
	//setup_firm_for_machine(machine);
	initialize_types();
	initialize_resolve();
}

static void finish_firmsmith() {
	finish_resolve();
	finish_types();
	ir_finish();
	//gen_firm_finish();
}

static void verify_no_dummy(ir_node *node, void *env) {
	(void)env;
	if (get_irn_opcode(node) == iro_Dummy) {
		fprintf(stderr, "Dummy node [%ld] found in block [%ld]\n",
			get_irn_node_nr(node), get_irn_node_nr(get_nodes_block(node)));
		assert(NULL && "Verify no dummy");
	}
}

static void generate_main_func() {
	ir_type *int_type = new_type_primitive(mode_Is);
	ir_type *type = new_type_method(0, 1, false, cc_cdecl_set, mtp_no_property);  // create the type
	set_method_res_type(type, 0, int_type);                                       // set the result type
	ir_entity *ent = new_entity(get_glob_type(), new_id_from_str("main"), type);
	ir_graph *fn_main = new_ir_graph(ent, 0);
	set_current_ir_graph(fn_main);

	// main just returns 0
	ir_node *node = new_Const(get_mode_null(mode_Is));
	ir_node *store = get_store();
	ir_node *results[1] = { node };
	ir_node *ret = new_Return(store, 1, results);
	add_immBlock_pred(get_irg_end_block(fn_main), ret);
	mature_immBlock(get_r_cur_block(fn_main));
	irg_finalize_cons(fn_main);
}

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

	/* Just to make the linker happy, create 'main' */
	generate_main_func();

	// Dump ir file
	char ir_file_name[256];
	snprintf(ir_file_name, sizeof ir_file_name, "%s.ir", fs_params.prog.strid);
	FILE *irout = fopen(ir_file_name, "w");
	ir_export_file(irout);
	fclose(irout);

	dump_all_ir_graphs(fs_params.prog.strid);

	for (size_t i = 0; i < ARR_LEN(prog->funcs); ++i) {
		irg_walk_graph(prog->funcs[i]->irg, verify_no_dummy, NULL, NULL);
	}
	
	/*
	(void)get_optimized_graph(get_current_ir_graph());
	assert(irg_verify(get_current_ir_graph()) && "valid graph");
	*/
		/*

	// Code generation
	FILE *out = fopen("main.s", "w");
	if(out == NULL) {
		fprintf(stderr, "couldn't open a.s for writing: %s\n", strerror(errno));
		return 1;
	}
	generate_code(out, "");
	fclose(out);
	

	// Stats
	if (!nostats) {
		print_cfg_stats(func->cfg);
		print_op_stats(func->cfg);
	}

	// finish libfirm
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
	initialize_firmsmith();

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

	finish_firmsmith();

	return ret;
}
