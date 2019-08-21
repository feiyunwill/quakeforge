/*
	expr_assign.c

	assignment expression construction and manipulations

	Copyright (C) 2001 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/06/15

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "qfcc.h"
#include "class.h"
#include "def.h"
#include "defspace.h"
#include "diagnostic.h"
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "idstuff.h"
#include "method.h"
#include "options.h"
#include "reloc.h"
#include "shared.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"
#include "value.h"
#include "qc-parse.h"

static expr_t *
check_assign_logic_precedence (expr_t *dst, expr_t *src)
{
	if (src->type == ex_expr && !src->paren && is_logic (src->e.expr.op)) {
		// traditional QuakeC gives = higher precedence than && and ||
		expr_t     *assignment;
		notice (src, "precedence of `=' and `%s' inverted for "
					 "traditional code", get_op_string (src->e.expr.op));
		// change {a = (b logic c)} to {(a = b) logic c}
		assignment = assign_expr (dst, src->e.expr.e1);
		assignment->paren = 1;	// protect assignment from binary_expr
		return binary_expr (src->e.expr.op, assignment, src->e.expr.e2);
	}
	return 0;
}

static expr_t *
check_valid_lvalue (expr_t *expr)
{
	switch (expr->type) {
		case ex_symbol:
			switch (expr->e.symbol->sy_type) {
				case sy_name:
					break;
				case sy_var:
					return 0;
				case sy_const:
					break;
				case sy_type:
					break;
				case sy_expr:
					break;
				case sy_func:
					break;
				case sy_class:
					break;
			}
			break;
		case ex_temp:
			return 0;
		case ex_expr:
			if (expr->e.expr.op == '.') {
				return 0;
			}
			if (expr->e.expr.op == 'A') {
				return check_valid_lvalue (expr->e.expr.e1);
			}
			break;
		case ex_uexpr:
			if (expr->e.expr.op == '.') {
				return 0;
			}
			if (expr->e.expr.op == 'A') {
				return check_valid_lvalue (expr->e.expr.e1);
			}
			break;
		case ex_state:
		case ex_bool:
		case ex_label:
		case ex_labelref:
		case ex_block:
		case ex_vector:
		case ex_nil:
		case ex_value:
		case ex_error:
			break;
	}
	if (options.traditional) {
		warning (expr, "invalid lvalue in assignment");
		return 0;
	}
	return error (expr, "invalid lvalue in assignment");
}

static expr_t *
check_types_compatible (expr_t *dst, expr_t *src)
{
	type_t     *dst_type = get_type (dst);
	type_t     *src_type = get_type (src);

	if (dst_type == src_type) {
		return 0;
	}

	if (type_assignable (dst_type, src_type)) {
		if (is_scalar (dst_type) && is_scalar (src_type)) {
			// the types are different but cast-compatible
			expr_t     *new = cast_expr (dst_type, src);
			// the cast was a no-op, so the types are compatible at the
			// low level (very true for default type <-> enum)
			if (new != src) {
				return assign_expr (dst, new);
			}
		}
		return 0;
	}
	// traditional qcc is a little sloppy
	if (!options.traditional) {
		return type_mismatch (dst, src, '=');
	}
	if (is_func (dst_type) && is_func (src_type)) {
		warning (dst, "assignment between disparate function types");
		return 0;
	}
	if (is_float (dst_type) && is_vector (src_type)) {
		warning (dst, "assignment of vector to float");
		src = field_expr (src, new_name_expr ("x"));
		return assign_expr (dst, src);
	}
	if (is_vector (dst_type) && is_float (src_type)) {
		warning (dst, "assignment of float to vector");
		dst = field_expr (dst, new_name_expr ("x"));
		return assign_expr (dst, src);
	}
	return type_mismatch (dst, src, '=');
}

static expr_t *
assign_vector_expr (expr_t *dst, expr_t *src)
{
	expr_t     *dx, *sx;
	expr_t     *dy, *sy;
	expr_t     *dz, *sz;
	expr_t     *dw, *sw;
	expr_t     *ds, *ss;
	expr_t     *dv, *sv;
	expr_t     *block;

	if (src->type == ex_vector) {
		src = convert_vector (src);
		if (src->type != ex_vector) {
			// src was constant and thus converted
			return assign_expr (dst, src);
		}
	}
	if (src->type == ex_vector && dst->type != ex_vector) {
		if (src->e.vector.type == &type_vector) {
			// guaranteed to have three elements
			sx = src->e.vector.list;
			sy = sx->next;
			sz = sy->next;
			dx = field_expr (dst, new_name_expr ("x"));
			dy = field_expr (dst, new_name_expr ("y"));
			dz = field_expr (dst, new_name_expr ("z"));
			block = new_block_expr ();
			append_expr (block, assign_expr (dx, sx));
			append_expr (block, assign_expr (dy, sy));
			append_expr (block, assign_expr (dz, sz));
			block->e.block.result = dst;
			return block;
		}
		if (src->e.vector.type == &type_quaternion) {
			// guaranteed to have two or four elements
			if (src->e.vector.list->next->next) {
				// four vals: x, y, z, w
				sx = src->e.vector.list;
				sy = sx->next;
				sz = sy->next;
				sw = sz->next;
				dx = field_expr (dst, new_name_expr ("x"));
				dy = field_expr (dst, new_name_expr ("y"));
				dz = field_expr (dst, new_name_expr ("z"));
				dw = field_expr (dst, new_name_expr ("w"));
				block = new_block_expr ();
				append_expr (block, assign_expr (dx, sx));
				append_expr (block, assign_expr (dy, sy));
				append_expr (block, assign_expr (dz, sz));
				append_expr (block, assign_expr (dw, sw));
				block->e.block.result = dst;
				return block;
			} else {
				// v, s
				sv = src->e.vector.list;
				ss = sv->next;
				dv = field_expr (dst, new_name_expr ("v"));
				ds = field_expr (dst, new_name_expr ("s"));
				block = new_block_expr ();
				append_expr (block, assign_expr (dv, sv));
				append_expr (block, assign_expr (ds, ss));
				block->e.block.result = dst;
				return block;
			}
		}
		internal_error (src, "bogus vector expression");
	}
	return 0;
}

static __attribute__((pure)) int
is_const_ptr (expr_t *e)
{
	if ((e->type != ex_value || e->e.value->lltype != ev_pointer)
		|| !(POINTER_VAL (e->e.value->v.pointer) >= 0
			 && POINTER_VAL (e->e.value->v.pointer) < 65536)) {
		return 1;
	}
	return 0;
}

static __attribute__((pure)) int
is_indirect (expr_t *e)
{
	if (e->type == ex_block && e->e.block.result)
		return is_indirect (e->e.block.result);
	if (e->type == ex_expr && e->e.expr.op == '.')
		return 1;
	if (!(e->type == ex_uexpr && e->e.expr.op == '.'))
		return 0;
	return is_const_ptr (e->e.expr.e1);
}

expr_t *
assign_expr (expr_t *dst, expr_t *src)
{
	int         op = '=';
	expr_t     *expr;
	type_t     *dst_type, *src_type;

	convert_name (dst);
	convert_name (src);

	if (dst->type == ex_error) {
		return dst;
	}
	if (src->type == ex_error) {
		return src;
	}

	if (options.traditional
		&& (expr = check_assign_logic_precedence (dst, src))) {
		return expr;
	}


	if ((expr = check_valid_lvalue (dst))) {
		return expr;
	}

	dst_type = get_type (dst);
	src_type = get_type (src);
	if (!dst_type) {
		internal_error (dst, "dst_type broke in assign_expr");
	}
	if (!src_type) {
		internal_error (src, "src_type broke in assign_expr");
	}

	if (is_pointer (dst_type) && is_array (src_type)) {
		// assigning an array to a pointer is the same as taking the address of
		// the array but using the type of the array elements
		src = address_expr (src, 0, src_type->t.fldptr.type);
		src_type = get_type (src);
	}
	if (src->type == ex_bool) {
		src = convert_from_bool (src, dst_type);
		if (src->type == ex_error) {
			return src;
		}
		src_type = get_type (src);
	}
	if (!is_void (dst_type) && src->type == ex_nil) {
		// nil is a type-agnostic 0
		// FIXME: assignment to compound types? error or memset?
		src_type = dst_type;
		convert_nil (src, src_type);
	}

	if ((expr = check_types_compatible (dst, src))) {
		// expr might be a valid expression, but if so, check_types_compatible
		// will take care of everything
		return expr;
	}

	if ((expr = assign_vector_expr (dst, src))) {
		return expr;
	}

	if (is_indirect (dst) && is_indirect (src)) {
		debug (dst, "here");
		if (is_struct (src_type)) {
			dst = address_expr (dst, 0, 0);
			src = address_expr (src, 0, 0);
			expr = new_move_expr (dst, src, src_type, 1);
		} else {
			expr_t     *temp = new_temp_def_expr (dst_type);

			expr = new_block_expr ();
			append_expr (expr, assign_expr (temp, src));
			append_expr (expr, assign_expr (dst, temp));
			expr->e.block.result = temp;
		}
		return expr;
	} else if (is_indirect (dst)) {
		debug (dst, "here");
		if (is_struct (dst_type)) {
			dst = address_expr (dst, 0, 0);
			src = address_expr (src, 0, 0);
			return new_move_expr (dst, src, dst_type, 1);
		}
		if (dst->type == ex_expr) {
			if (get_type (dst->e.expr.e1) == &type_entity) {
				dst_type = dst->e.expr.type;
				dst->e.expr.type = pointer_type (dst_type);
				dst->e.expr.op = '&';
			}
			op = PAS;
		} else {
			if (is_const_ptr (dst->e.expr.e1)) {
				dst = dst->e.expr.e1;
				op = PAS;
			}
		}
	} else if (is_indirect (src)) {
		debug (dst, "here");
		if (is_struct (dst_type)) {
			dst = address_expr (dst, 0, 0);
			src = address_expr (src, 0, 0);
			src->rvalue = 1;
			return new_move_expr (dst, src, src_type, 1);
		}
		if (src->type == ex_uexpr) {
			expr = src->e.expr.e1;
			if (is_const_ptr (expr)) {
				if (expr->type == ex_expr && expr->e.expr.op == '&'
					&& expr->e.expr.type->type == ev_pointer
					&& !is_constant (expr)) {
					src = expr;
					src->e.expr.op = '.';
					src->e.expr.type = src_type;
					src->rvalue = 1;
				}
			}
		}
	}

	if (is_struct (dst_type)) {
		return new_move_expr (dst, src, dst_type, 0);
	}

	expr = new_binary_expr (op, dst, src);
	expr->e.expr.type = dst_type;
	return expr;
}
