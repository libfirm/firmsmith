#include "types.h"
#include <libfirm/adt/array.h>

int n_modes;
static ir_mode **modes = NULL;

int n_primitives;
static ir_type **primitive_types = NULL;
static ir_type **compound_types  = NULL;
static ir_entity **entities      = NULL;

static void print_type(ir_type *type);

static void create_modes() {
    n_modes = 10;
    modes = calloc(n_modes, sizeof(ir_mode*));
    int i = 0;
    modes[i++] =  mode_b;
    modes[i++] =  mode_P;
    //modes[i++] =  mode_F;
    //modes[i++] =  mode_D;
    modes[i++] =  mode_Bs;
    modes[i++] =  mode_Bu;
    modes[i++] =  mode_Hs;
    modes[i++] =  mode_Hu;
    modes[i++] =  mode_Is;
    modes[i++] =  mode_Iu;
    modes[i++] =  mode_Ls;
    modes[i++] =  mode_Lu;
    assert(i == n_modes);
}

static void create_primitive_types() {
    n_primitives = n_modes;
    primitive_types = calloc(n_primitives, sizeof(ir_type*));
    for (int i = 0; i < n_primitives; ++i) {
        ir_type *type = new_type_primitive(modes[i]);
        if (mode_is_float(modes[i])) {
            set_type_alignment(type, 4);
        }
        primitive_types[i] = type;
    }
}

static ir_entity *get_registered_entity(ir_type *owner, ir_type *type) {
    ir_entity* ent = new_entity(owner, id_unique("fs_entity"), type);
    ARR_APP1(ir_entity*, entities, ent);
    return ent;
}

static ir_type *get_registered_struct() {
    ir_type *type = new_type_struct(id_unique("fs_struct"));
    ARR_APP1(ir_type*, compound_types, type);
    return type;
}

static void create_struct_type() {
    ir_type *type = get_registered_struct();
    int offset = 0;
    int n_members = 3 + rand() % 5;
    for (int i = 0; i < n_members; ++i) {
        int r = rand() % 4;
        if (r == 0 && ARR_LEN(compound_types) > 1) {
            // 25% chance to pick compound entity
            int compound_idx = compound_idx = rand() % (ARR_LEN(compound_types) - 1);
            ir_type *member_type = compound_types[compound_idx];
            ir_entity *member_ent = get_registered_entity(type, member_type);
            set_entity_offset(member_ent, offset);
            offset += get_type_size(member_type);
        } else {
            // 75% change to primitive
            ir_type *member_type  = get_random_prim_type();
            ir_entity *member_ent = get_registered_entity(type, member_type);
            set_entity_offset(member_ent, offset);
            offset += get_mode_size_bytes(get_type_mode(member_type));
        }
    }
    set_type_state(type, layout_fixed);
    set_type_size(type, offset);
}

static ir_type *get_registered_union() {
    ir_type *type = new_type_union(id_unique("fs_union"));
    ARR_APP1(ir_type*, compound_types, type);
    return type;
}

static void create_union_type() {
    ir_type *type = get_registered_union();
    int max_size = 0;
    int n_members = 3 + rand() % 5;
    for (int i = 0; i < n_members; ++i) {
        int r = rand() % 4;
        if (r == 0 && ARR_LEN(compound_types) > 1) {
            // 25% chance to pick compound entity
            int compound_idx = compound_idx = rand() % (ARR_LEN(compound_types) - 1);
            ir_type *member_type = compound_types[compound_idx];
            ir_entity *member_ent = get_registered_entity(type, member_type);
            set_entity_offset(member_ent, 0);
            int member_size = get_type_size(member_type);
            if (member_size > max_size) {
                max_size = member_size;
            }
        } else {
            // 75% change to primitive
            ir_type *member_type  = get_random_prim_type();
            ir_entity *member_ent = get_registered_entity(type, member_type);
            set_entity_offset(member_ent, 0);
            int member_size = get_mode_size_bytes(get_type_mode(member_type));
            if (member_size > max_size) {
                max_size = member_size;
            }
        }
    }
    set_type_state(type, layout_fixed);
    set_type_size(type, max_size);
}


ir_type* get_pointer_type(void) {
    return primitive_types[0];
}

ir_type* get_bool_type(void) {
    return primitive_types[1];
}

ir_type* get_int_type(void) {
    return primitive_types[2];
}

ir_type *get_random_prim_type(void) {
    return primitive_types[rand() % (n_primitives - 2) + 2];
}

static void print_type_core(ir_type *type, int indent) {
    for (int i = 0; i < indent; ++i) {
        printf("  ");
    }
    if (is_compound_type(type)) {
        char const *kind = is_Struct_type(type) ? "struct" : "union"; 
        printf("%s %s (size = %d) {\n",
            kind, (char*)get_compound_ident(type),
            get_type_size(type)
        );
        for (size_t i = 0; i < get_compound_n_members(type); ++i) {
            ir_entity *ent = get_compound_member(type, i);
            print_type_core(get_entity_type(ent), indent + 1);
            printf(" %s;\n", get_entity_ident(ent));
        }
        for (int i = 0; i < indent; ++i) {
            printf("  ");
        }
        printf("}");
    } else if (is_Primitive_type(type)) {
        printf("%s", get_mode_name(get_type_mode(type)));
    }
}

static void print_type(ir_type *type) {
    print_type_core(type, 0);
    printf("\n");
}

ir_entity *get_associated_entity(ir_type *type) {
    // TODO: randomize picked entity if more candidates
    for (size_t i = 0; i < ARR_LEN(entities); ++i) {
        ir_entity *ent = entities[i];
        if (get_entity_type(ent) == type) {
            return ent;
        }
    }
    return NULL;
}

/**
  * Initialize types module
  **/
void initialize_types(void) {
    create_modes();
    create_primitive_types();
    entities = NEW_ARR_F(ir_entity*, 0);
    compound_types = NEW_ARR_F(ir_type*, 0);
    for (int i = 0; i < 10; ++i) {
        if (rand() % 2 == 0) {
            create_union_type();
        } else {
            create_struct_type();
        }
    }
    for (size_t i = 0; i < ARR_LEN(compound_types); ++i) {
        print_type(compound_types[i]);
    }
}


/**
  * Clean up data allocated by types module
  **/
void finish_types(void) {

}
