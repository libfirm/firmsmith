#include "optimizations.h"

typedef enum opt_target {
	OPT_TARGET_IRG, /**< optimization function works on a single graph */
	OPT_TARGET_IRP  /**< optimization function works on the complete program */
} opt_target_t;


typedef void (*transform_irg_func)(ir_graph *irg);
typedef void (*transform_irp_func)(void);

typedef enum opt_flags {
	OPT_FLAG_NONE         = 0,
	OPT_FLAG_ENABLED      = 1 << 0, /**< enable the optimization */
	OPT_FLAG_NO_DUMP      = 1 << 1, /**< don't dump after transformation */
	OPT_FLAG_NO_VERIFY    = 1 << 2, /**< don't verify after transformation */
	OPT_FLAG_HIDE_OPTIONS = 1 << 3, /**< do not automatically process
	                                     -foptions for this transformation */
	OPT_FLAG_ESSENTIAL    = 1 << 4, /**< output won't work without this pass
	                                     so we need it even with -O0 */
} opt_flags_t;

typedef struct {
	opt_target_t  target;
	const char   *name;
	union {
		transform_irg_func transform_irg;
		transform_irp_func transform_irp;
	} u;
	const char   *description;
	opt_flags_t   flags;
} opt_config_t;

static opt_config_t opts[] = {
#define IRG(a, b, c, d) { OPT_TARGET_IRG, a, .u.transform_irg = b, c, d }
#define IRP(a, b, c, d) { OPT_TARGET_IRP, a, .u.transform_irp = b, c, d }
	IRG("bool",              opt_bool,                 "bool simplification",                                   OPT_FLAG_NONE),
	IRG("combo",             combo,                    "combined CCE, UCE and GVN",                             OPT_FLAG_NONE),
	IRG("confirm",           construct_confirms,       "confirm optimization",                                  OPT_FLAG_HIDE_OPTIONS),
	IRG("control-flow",      optimize_cf,              "optimization of control-flow",                          OPT_FLAG_HIDE_OPTIONS),
	IRG("dead",              dead_node_elimination,    "dead node elimination",                                 OPT_FLAG_HIDE_OPTIONS | OPT_FLAG_NO_DUMP | OPT_FLAG_NO_VERIFY),
	IRG("deconv",            conv_opt,                 "conv node elimination",                                 OPT_FLAG_NONE),
	IRG("occults",           occult_consts,            "occult constant folding",                               OPT_FLAG_NONE),
	IRG("frame",             opt_frame_irg,            "remove unused frame entities",                          OPT_FLAG_NONE),
	//IRG("gvn-pre",           do_gvn_pre,               "global value numbering partial redundancy elimination", OPT_FLAG_NONE),
	IRG("if-conversion",     opt_if_conv,              "if-conversion",                                         OPT_FLAG_NONE),
	//IRG("invert-loops",      do_loop_inversion,        "loop inversion",                                        OPT_FLAG_NONE),
	//IRG("ivopts",            do_stred,                 "induction variable strength reduction",                 OPT_FLAG_NONE),
	IRG("local",             optimize_graph_df,        "local graph optimizations",                             OPT_FLAG_HIDE_OPTIONS),
	IRG("lower",             lower_highlevel_graph,    "lowering",                                              OPT_FLAG_HIDE_OPTIONS | OPT_FLAG_ESSENTIAL),
	//IRG("lower-mux",         lower_mux,             "mux lowering",                                          OPT_FLAG_NONE),
	IRG("opt-load-store",    optimize_load_store,      "load store optimization",                               OPT_FLAG_NONE),
	IRG("memcombine",        combine_memops,           "combine adjacent memory operations",                    OPT_FLAG_NONE),
	IRG("opt-tail-rec",      opt_tail_rec_irg,         "tail-recursion elimination",                            OPT_FLAG_NONE),
	IRG("parallelize-mem",   opt_parallelize_mem,      "parallelize memory",                                    OPT_FLAG_NONE),
	//fIRG("gcse",              do_gcse,                  "global common subexpression elimination",               OPT_FLAG_NONE),
	IRG("place",             place_code,               "code placement",                                        OPT_FLAG_NONE),
	IRG("reassociation",     optimize_reassociation,   "reassociation",                                         OPT_FLAG_NONE),
	IRG("remove-confirms",   remove_confirms,          "confirm removal",                                       OPT_FLAG_HIDE_OPTIONS | OPT_FLAG_NO_DUMP | OPT_FLAG_NO_VERIFY),
	IRG("remove-phi-cycles", remove_phi_cycles,        "removal of phi cycles",                                 OPT_FLAG_HIDE_OPTIONS),
	IRG("scalar-replace",    scalar_replacement_opt,   "scalar replacement",                                    OPT_FLAG_NONE),
//	IRG("shape-blocks",      shape_blocks,             "block shaping",                                         OPT_FLAG_NONE),
	IRG("thread-jumps",      opt_jumpthreading,        "path-sensitive jumpthreading",                          OPT_FLAG_NONE),
	IRG("unroll-loops",      do_loop_unrolling,        "loop unrolling",                                        OPT_FLAG_NONE),
	IRG("vrp",               set_vrp_data,             "value range propagation",                               OPT_FLAG_NONE),
	//IRG("rts",               rts_map,                  "optimization of known library functions",               OPT_FLAG_NONE),
	//IRP("inline",            do_inline,                "inlining",                                              OPT_FLAG_NONE),
	IRP("lower-const",       lower_const_code,         "lowering of constant code",                             OPT_FLAG_HIDE_OPTIONS | OPT_FLAG_NO_DUMP | OPT_FLAG_NO_VERIFY | OPT_FLAG_ESSENTIAL),
	IRP("local-const",       local_opts_const_code,    "local optimisation of constant initializers",
	                            OPT_FLAG_HIDE_OPTIONS | OPT_FLAG_NO_DUMP | OPT_FLAG_NO_VERIFY | OPT_FLAG_ESSENTIAL),
	IRP("target-lowering",   be_lower_for_target,      "lowering necessary for target architecture",            OPT_FLAG_HIDE_OPTIONS | OPT_FLAG_ESSENTIAL),
	IRP("opt-func-call",     optimize_funccalls,       "function call optimization",                            OPT_FLAG_NONE),
	//IRP("opt-proc-clone",    do_cloning,               "procedure cloning",                                     OPT_FLAG_NONE),
	IRP("remove-unused",     garbage_collect_entities, "removal of unused functions/variables",                 OPT_FLAG_NO_DUMP | OPT_FLAG_NO_VERIFY),
	IRP("opt-cc",            mark_private_methods,     "calling conventions optimization",                      OPT_FLAG_NONE),
#undef IRP
#undef IRG
};

ir_graph *get_optimized_graph(ir_graph *irg) {
    for (size_t i = 0; i < sizeof(opts)  / sizeof(opts[0]); ++i) {
        opt_config_t config = opts[i];
        if (config.target == OPT_TARGET_IRG) {
            config.u.transform_irg(irg);
        }
    }
    return irg;
}
