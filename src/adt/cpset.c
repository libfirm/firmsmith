/*
 * Copyright (C) 1995-2008 University of Karlsruhe.  All right reserved.
 *
 * This file is part of libFirm.
 *
 * This file may be distributed and/or modified under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * Licensees holding valid libFirm Professional Edition licenses may use
 * this file in accordance with the libFirm Commercial License.
 * Agreement provided with the Software.
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.
 */

/**
 * @file
 * @brief   Custom pointer set
 * @author  Matthias Braun
 * @version $Id: cpset.c 17143 2008-01-02 20:56:33Z beck $
 *
 * This implements a set of pointers which allows to specify custom callbacks
 * for comparing and hashing it's elements.
 */
#include "cpset.h"

#define HashSet                   cpset_t
#define HashSetIterator           cpset_iterator_t
#define HashSetEntry              cpset_hashset_entry_t
#define ValueType                 void*
#define NullValue                 NULL
#define DeletedValue              ((void*)-1)
#define Hash(this,key)            this->hash_function(key)
#define KeysEqual(this,key1,key2) this->cmp_function(key1, key2)
#define SCALAR_RETURN
#define SetRangeEmpty(ptr,size)   memset(ptr, 0, (size) * sizeof(cpset_hashset_entry_t))

void cpset_init_(cpset_t *set);
#define hashset_init            cpset_init_
void cpset_init_size_(cpset_t *set, size_t size);
#define hashset_init_size       cpset_init_size_
#define hashset_destroy         cpset_destroy
#define hashset_insert          cpset_insert
#define hashset_remove          cpset_remove
#define hashset_find            cpset_find
#define hashset_size            cpset_size
#define hashset_iterator_init   cpset_iterator_init
#define hashset_iterator_next   cpset_iterator_next
#define hashset_remove_iterator cpset_remove_iterator

#include "hashset.c.h"

void cpset_init(cpset_t *set, cpset_hash_function hash_function,
                cpset_cmp_function cmp_function)
{
	set->hash_function = hash_function;
	set->cmp_function = cmp_function;
	cpset_init_(set);
}

void cpset_init_size(cpset_t *set, cpset_hash_function hash_function,
                     cpset_cmp_function cmp_function, size_t expected_elems)
{
	set->hash_function = hash_function;
	set->cmp_function = cmp_function;
	cpset_init_size_(set, expected_elems);
}
