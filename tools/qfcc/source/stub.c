#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "class.h"
#include "codespace.h"
#include "diagnostic.h"
#include "debug.h"
#include "def.h"
#include "defspace.h"
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "obj_file.h"
#include "options.h"
#include "qfcc.h"
#include "strpool.h"
#include "type.h"
#include "value.h"

struct dstring_s;
options_t options;
int num_linenos;
pr_lineno_t *linenos;
pr_info_t pr;
__attribute__((const)) string_t ReuseString (const char *str) {return 0;}
void encode_type (struct dstring_s *str, const type_t *type) {}
__attribute__((const)) codespace_t *codespace_new (void) {return 0;}
void codespace_addcode (codespace_t *codespace, struct dstatement_s *code, int size) {}
__attribute__((const)) int function_parms (function_t *f, byte *parm_size) {return 0;}
void def_to_ddef (def_t *def, ddef_t *ddef, int aux) {}
__attribute__((const)) expr_t *_warning (expr_t *e, const char *file, int line, const char *fmt, ...) {return 0;}
__attribute__((const)) expr_t *_error (expr_t *e, const char *file, int line, const char *fmt, ...) {return 0;}
