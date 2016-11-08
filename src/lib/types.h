#ifndef TYPES_H
#define TYPES_H

#include <libfirm/firm.h>

void initialize_types(void);
void finish_types(void);

ir_type* get_pointer_type(void);
ir_type* get_bool_type(void);
ir_type *get_int_type(void);
ir_type *get_random_prim_type(void);
ir_entity *get_associated_entity(ir_type *type);

#endif
