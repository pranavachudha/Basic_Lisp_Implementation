#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* Forward Declarations */
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;


/* Forward Declarations */
void lval_del(lval* a);
lval* lval_copy(lval* a);
lval* lval_err(char* fmt, ...);
void lval_print(lval* v, lenv* e);
lval* lval_eval(lenv* e, lval* v);
lval* lval_take(lval* v, int i);
lval* lval_pop(lval* v, int i);
lval* builtin_op(lenv* e, lval* a, char* op);
lval* builtin(lval* a, char* func);
lval* builtin_env(lenv* e, lval* a);
lval* builtin_eval(lenv* e, lval* a);
lval* builtin_list(lenv* e, lval* a);
char* ltype_name(int t);


#define LASSERT(args, cond, fmt, ...) \
	if (!(cond)) { \
		lval* err = lval_err(fmt, ##__VA_ARGS__);\
		lval_del(args); \
		return err; \
}

#define LASSERT_TYPE(func, args, index, expect) \
	LASSERT(args, args->cell[index]->type == expect, \
	 "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.",\
	 func, index, ltype_name(args->cell[index]->type), ltype_name(expect))

#define LASSERT_NOA(func, args, num) \
	LASSERT(args, args->count == num, \
	 "Function %s passed incorrect number of arguments. Got %i, Expected %i.", \
	 func, args->count, num)

#define LASSERT_ELIST(func, args, index) \
	LASSERT(args, args->cell[index]->count != 0, \
	 "Function '%s' passed {} for argument %i.", func, index);

/* Create Enumberation of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };


/* If we are compiling on windows compile these functions */
#ifdef _WIN32
#include <string.h>

static char buffer[2048];

/* dummy readline function */
char* readline(char* prompt){
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer)+1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\0';
	return cpy;
}

/* dummy add_history function */
void add_history(char* unused){}

#else
#include <editline/readline.h>
#endif

typedef lval*(*lbuiltin)(lenv*, lval*);

/* Declare New lval(lisp value) struct */
struct lval{
	int type;
	double num;
	/*Error and symbol types have some string data */
	char* err;
	char* sym;

	/* Function */
	lbuiltin builtin;
	lenv* env;
	lval* formals;
	lval* body;
	/* Count and Pointer to a list of "lval*" */
	int count;
	lval** cell;
};

struct lenv {
	lenv* par;
	int count;
	char** syms;
	lval** vals;
};

lenv* lenv_new(void) {
	lenv* e = malloc(sizeof(lenv));
	e->par = NULL;
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

void lenv_del(lenv* e) {
	for (int i = 0; i < e->count; i++) {
		free(e->syms[i]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

lval* lenv_get(lenv* e, lval* k) {
	/* Iterate over all items in environment */
	for (int i = 0; i < e->count; i++) {
		/* Check if the Stored string matches the symbol string */
		/* If it does, return a copy of the value */
		if (strcmp(e->syms[i], k->sym) == 0) {
			return lval_copy(e->vals[i]);
		}
	}

	/* If no symbol check in parent otherwise error */
	if (e->par) {
		return lenv_get(e->par, k);
	}
	else {
		return lval_err("Unbound Symbol '%s'", k->sym);
	}

	return lval_err("Unbound Symbol '%s'", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {
	
	/* Iterate over all the items in environment */
	/* This is to see if variable already exists */
	for (int i = 0; i < e->count; i++) {

		/* If variable is found delete item at that position */
		/* And replace with variable supplied by user */
		if (strcmp(e->syms[i], k->sym) == 0) {
			lval_del(e->vals[i]);
			e->vals[i] = lval_copy(v);
			return;
		}
	}

	/* If no existing entry found allocate space for new entry */
	e->count++;
	e->vals = realloc(e->vals, sizeof(lval*) * e->count);
	e->syms = realloc(e->syms, sizeof(char*) * e->count);

	/* Copy contents of lval and symbol string into new location */
	e->vals[e->count-1] = lval_copy(v);
	e->syms[e->count-1] = malloc(strlen(k->sym) + 1);
	strcpy(e->syms[e->count-1], k->sym);
}

lenv* lenv_copy(lenv* e) {
	lenv* n = malloc(sizeof(lenv));
	n->par = e->par;
	n->count = e->count;
	n->syms = malloc(sizeof(char*) * n->count);
	n->vals = malloc(sizeof(lval*) * n->count);
	for (int i = 0; i < e->count; i++) {
		n->syms[i] = malloc(strlen(e->syms[i]) + 1);
		strcpy(n->syms[i], e->syms[i]);
		n->vals[i] = lval_copy(e->vals[i]);
	}
	return n;
}

void lenv_def (lenv* e, lval* k, lval* v) {
	/* Iterate till e has no parent */
	while (e->par) { e = e->par; }
	/* Put value in e */
	lenv_put(e, k, v);
}

/* Construct a pointer to a new Number lval */
lval* lval_num(double x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

/* Construct a pointer to a new Error lval */
lval* lval_err(char* fmt, ...) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	

	/*Create a va list and initialize it */
	va_list va;
	va_start(va, fmt);

	/* Allocate 512 bytes of space */
	v->err = malloc(512);

	/* printf the error string with a maximum of 511 characters */
	vsnprintf(v->err, 511, fmt, va);

	/* Reallocate to number of bytes actually used*/
	v->err = realloc(v->err, strlen(v->err) + 1);

	/* Cleanup our va list */
	va_end(va);	

	return v;
}

/* Construct a pointer to a new Symbol lval */
lval* lval_sym(char* s) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

/* Construct a pointer to a new Function lval */
lval* lval_fun(lbuiltin func) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->builtin = func;
	return v;
}

/* Construct a function to return lval with user defined function's values */
lval* lval_lambda(lval* formals, lval* body) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;

	/* Set Builtin to NULL */
	v->builtin = NULL;

	/* Build new environment */
	v->env = lenv_new();

	/* Set Formals and Body */
	v->formals = formals;
	v->body = body;
	return v;
}

/* A pointer to a new empty Sexpr lval */
lval* lval_sexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

/*A pointer to a new empty Qexpr lval */
lval* lval_qexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_read_num(mpc_ast_t* t) {
	errno = 0;
	double x = strtod(t->contents, NULL);
	return errno != ERANGE ?
		lval_num(x): lval_err("Invalid Number");
}

lval* lval_add(lval* v, lval* x) {
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	v->cell[v->count-1] = x;
	return v;
}
	


lval* lval_read(mpc_ast_t* t) {                            
	/*If Symbol or Number return conversion to that type */
	if (strstr(t->tag, "number")) { return lval_read_num(t); }
	if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

	/* if root (>) or sexpr then create empty list */
	lval* x = NULL;
	if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
	if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
	if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }
	/* Fill this list with any valid expression contained within */
	for (int i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
		if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
		x = lval_add(x, lval_read(t->children[i]));
	}

	return x;
}

lval* lval_copy(lval* v) {
	lval* x = malloc(sizeof(lval));
	x->type = v->type;

	switch (v->type) {
		/* Copy Functions and Numbers Directly */
		case LVAL_FUN:
			if (v->builtin) {
				x->builtin = v->builtin; break;
			} else {
				x->builtin = NULL;
				x->env = lenv_copy(v->env);
				x->formals = lval_copy(v->formals);
				x->body = lval_copy(v->body);
			}
		break;

		case LVAL_NUM: x->num = v->num; break;

		/* Copy Strings using malloc and strcpy */
		case LVAL_ERR:
			x->err = malloc(strlen(v->err) + 1);
			strcpy(x->err, v->err); break;

		case LVAL_SYM:
			x->sym = malloc(strlen(v->sym) + 1);
			strcpy(x->sym, v->sym); break;

		/*Copy Lists by copying each sub-expression */
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			x->count = v->count;
			x->cell = malloc(sizeof(lval*) * x->count);
			for (int i = 0; i < x->count; i++) {
				x->cell[i] = lval_copy(v->cell[i]);
			}
		break;
	}

	return x;
}


void lval_del(lval* v) {
	switch (v->type) {
		/* Do nothing special for number type (since it doesn't use a pointer) */
		case LVAL_NUM: break;

		/* For Err or Sym free the string data */
		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;
		case LVAL_FUN:
			if (!v->builtin) {
				lenv_del(v->env);
				lval_del(v->formals);
				lval_del(v->body);
			}
		break;
		/* If Qexpr or Sexpr then delete all elements inside */
		case LVAL_QEXPR:
		case LVAL_SEXPR:
			       for (int i = 0; i < v->count; i++) {
				       lval_del(v->cell[i]);
			       }
			/* Also free the memory allocated to contain the pointers */
			       free(v->cell);
		break;
	}
	/* Free the memory allocated for the "lval" struct itself */
	free(v);
}

lval* lval_call(lenv* e, lval* f, lval* a) {
	/*If Builtin then simply call that */
	if (f->builtin) { return f->builtin(e, a); }

	/* Record Argument Counts*/
	int given = a->count;
	int total = f->formals->count;

	/* While arguments still remain to be processed */
	while (a->count) {

		/*If we've ran out of formal arguments to bind */
		if (f->formals->count == 0) {
			lval_del(a); return lval_err(
				"Function passed too many arguments. "
				"Got %i, Expected %i.", given, total);
		}

		/* Pop the first symbol from the formals */
		lval* sym = lval_pop(f->formals, 0);

		/* Special Case to deal with '&' */
		if (strcmp(sym->sym, "&") == 0) {

			/* Ensure '&' is followed by another symbol */
			if (f->formals->count != 2) {
				lval_del(a);
				return lval_err("Function format invalid. "
				"Symbol '&' not followed by single symbol.");
			}

			/* Next formal should be bound to remaining arguments */
			lval* nsym = lval_pop(f->formals, 0);
			lenv_put(f->env, nsym, builtin_list(e, a));
			lval_del(sym); lval_del(nsym);
			break;
		}

		/* Pop the next argument from the list */
		lval* val = lval_pop(a, 0);

		/* Bind a copy into the function's environment */
		lenv_put(f->env, sym, val);

		/* Delete symbol and value */
		lval_del(sym); lval_del(val);
	}

	/* If '&' remains in formal list bind to empty list */
	if (f->formals->count > 0 &&
	strcmp(f->formals->cell[0]->sym, "&") == 0) {

		/* Check to ensure that & is not passed invalidly.*/
		if (f->formals->count != 2) {
			return lval_err("Function format invalid. "
			"Symbol '&' not followed by single symbol.");
		}

		/* Pop and delete '&' symbol */
		lval_del(lval_pop(f->formals, 0));

		/* Pop next symbol and create empty list */
		lval* sym = lval_pop(f->formals, 0);
		lval* val = lval_qexpr();

		/* Bind to environment and delete */
		lenv_put(f->env, sym, val);
		lval_del(sym); lval_del(val);
	}
	/* Argument list is now bound so can be cleaned up */
	lval_del(a);

	/* If all formals have been bound evaluate */
	if (f ->formals->count == 0) {

		/*Set environment parent to evaluation environment */
		f->env->par = e;

		/* Evaluate and return */
		return builtin_eval(
			f->env, lval_add(lval_sexpr(), lval_copy(f->body)));
		
	} else {
		/*Otherwise return partially evaluated function */
		return lval_copy(f);
	}
}

void lval_expr_print(lval* v, char open, char close) {
	putchar(open);
	for (int i = 0; i < v->count; i++) {
      		lval_print(v->cell[i], NULL);

		if (i != (v->count-1)) {
			putchar(' ');
		}
      	}
	putchar(close);
}



/* Print an "lval" */
void lval_print(lval* v, lenv* e) {
	switch(v->type) {
		/* if the v.type value is a number print it and break out */
		case LVAL_NUM: printf("%.6g", v->num); break;
		case LVAL_ERR: printf("Error: %s", v->err); break;
		case LVAL_SYM: printf("%s", v->sym); break;
		case LVAL_FUN:
			if (v->builtin) {
				for (int i = 0; i < e->count; i++) {
					if (e->vals[i]->builtin == *(v->builtin)){
						printf("%s", e->syms[i]);
					}
				}
			} else {
				printf("(\\ "); lval_print(v->formals, NULL);
				putchar(' '); lval_print(v->body, NULL); putchar(')');
			}
			break;
		case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
		case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
	}
}



/* Print an "lval" followed by a newline */
void lval_println(lval* v, lenv* e) { lval_print(v, e); putchar('\n'); }


/* Uses recursion to traverse the tree until leaf 
 * nodes of every branch to count total nodes */
int number_of_nodes(mpc_ast_t* t) {
	if (t->children_num == 0) { return 1; }
	if (t->children_num >= 1) {
		int total = 1;
		for (int i = 0; i < t->children_num; i++) {
			total = total + number_of_nodes(t->children[i]);
			
		}
		return total;
	}

	return 0;
}

/* Counts the number of leaves in the tree */
int number_leaf_nodes(mpc_ast_t* t) {
	if (t->children_num == 0) {
		return 1;
	}

	if (t->children_num >= 1){
		int leaf_nodes = 0;
		for (int i = 0; i < t->children_num; i++)
			leaf_nodes += number_leaf_nodes(t->children[i]);
		return leaf_nodes;
	}
	
	return 0;
}

lval* lval_eval_sexpr(lenv* e, lval* v){
	/* Evaluate Children */
	for (int i = 0; i < v->count; i++){
		if(v->cell[i]->type == LVAL_SYM){
			if(strcmp(v->cell[i]->sym, "env") == 0) { return builtin_env(e, v);}
		}
		v->cell[i] = lval_eval(e, v->cell[i]);
	}

	/* Error Checking */
	for (int i = 0; i < v->count; i++) {
		if(v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
	}

	/* Empty Expression */
	if (v->count == 0) { return v; }

	/* Single Expression */
	if (v->count == 1) { return lval_take(v, 0); }

	/* Ensure First Element is Symbol */
	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_FUN) {
		lval* err = lval_err(
			"S-Expression starts with incorrect type. "
			"Got %s, Expected %s.",
			ltype_name(f->type), ltype_name(LVAL_FUN));
		lval_del(f); lval_del(v);
		return err;
	}


	/* Call builtin with operator */
	lval* result = lval_call(e, f ,v);
	lval_del(f);
	return result;
}


lval* lval_eval(lenv* e, lval* v) {
	if (v->type == LVAL_SYM) {
		lval* x = lenv_get(e, v);
		lval_del(v);
		return x;
	}
	/* Evaluate S-expressions */
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
	if (v->type == LVAL_QEXPR) { return v; }
	/* All other lval types remain the same */
	return v;
}

lval* lval_pop(lval* v, int i) {
	/* Find the item at "i" */
	lval* x = v->cell[i];

	/* Shift memory after the item at "i" over the top */
	memmove(&v->cell[i], &v->cell[i+1],
	 sizeof(lval*) * (v->count-i-1));

	/* Decresae the count of items in the list */
	v->count--;

	/* Reallocate the memory used */
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	return x;

}

lval* lval_take(lval* v, int i) {
	lval* x = lval_pop(v, i);
	lval_del(v);
	return x;
}

lval* builtin_op(lenv* e, lval* a, char* op) {

	/* Ensure all arguments are numbers */
	for (int i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			lval_del(a);
			return lval_err("Cannot operate on non-number!");
		}
	}

	/* Pop the first element */
	lval* x = lval_pop(a, 0);

	/* If no arguments and sub then perform unary negation */
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		x->num = -x->num;
	}

	/*While there are still elements remaining */
	while(a->count > 0) {
		/* Pop the next element */
		lval* y = lval_pop(a, 0);
		if(strcmp(op, "min") == 0) { x->num = MIN(x->num, y->num); }
		if(strcmp(op, "max") == 0) { x->num = MAX(x->num, y->num); }
		if(strcmp(op, "+") == 0) { x->num += y->num; }
		if(strcmp(op, "-") == 0) { x->num -= y->num; }
		if(strcmp(op, "*") == 0) { x->num *= y->num; }
		if(strcmp(op, "^") == 0) { x->num = pow(x->num, y->num); }
		if(strcmp(op, "/") == 0) {
			if(y->num == 0) {
			lval_del(x); lval_del(y);
			x = lval_err("Division By Zero!"); break;
			}
			x->num /= y->num;
		}
		if(strcmp(op, "%") == 0) {
			if(y->num == 0) {
				lval_del(x); lval_del(y);
				x = lval_err("Division By Zero!"); break;
			}

			if(y->num == (int) y->num && x->num == (int) x->num){
				int temp_x = (int) x->num;
				int temp_y = (int) y->num;
				temp_x %= temp_y;
				x->num = temp_x;
			}

			else{
				lval_del(x); lval_del(y);
				x = lval_err("Cannot use remainder operator with decimal numbers"); break;
			}
		}
		
		lval_del(y);
	}

	lval_del(a); return x;
	
}

lval* builtin_add(lenv* e, lval* a) {
	return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
	return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
	return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
	return builtin_op(e, a, "/");
}

lval* builtin_rem(lenv* e, lval* a) {
	return builtin_op(e, a, "%");
}

lval* builtin_min(lenv* e, lval* a) {
	return builtin_op(e, a, "min");
}

lval* builtin_max(lenv* e, lval* a) {
	return builtin_op(e, a, "max");
}

char* ltype_name(int t) {
	switch(t) {
		case LVAL_FUN: return "Function";
		case LVAL_NUM: return "Number";
		case LVAL_ERR: return "Error";
		case LVAL_SYM: return "Symbol";
		case LVAL_SEXPR: return "S-Expression";
		case LVAL_QEXPR: return "Q-Expression";
		default: return "Unknown";
	}
}
lval* builtin_head(lenv* e, lval* a) {
	/* Check Error Conditions */
	LASSERT_NOA("head", a, 1);
	LASSERT_TYPE("head", a, 0, LVAL_QEXPR);
	LASSERT_ELIST("head", a, 0);

	lval* v = lval_take(a, 0);

	/* Delete all elements that are not head and return */
	while (v->count > 1) { lval_del(lval_pop(v, 1)); }
	return v;
}

lval* builtin_tail(lenv* e, lval* a) {
	/* Check Error Conditions */
	LASSERT_NOA("tail", a, 1);
	LASSERT_TYPE("tail", a, 0, LVAL_QEXPR);
	LASSERT_ELIST("tail", a, 0);

	lval* v = lval_take(a, 0);

	lval_del(lval_pop(v, 0));
	return v;
}

lval* builtin_list(lenv* e, lval* a) {
	a->type = LVAL_QEXPR;
	return a;
}

lval* builtin_eval(lenv* e, lval* a) {
	LASSERT_NOA("eval", a, 1);
	LASSERT_TYPE("eval", a, 0, LVAL_QEXPR);

	lval* x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y) {

	/* For each cell in 'y' add it to 'x' */
	while (y->count) {
		x = lval_add(x, lval_pop(y, 0));
	}

	/* Delete the empty 'y' and return 'x' */
	lval_del(y);
	return x;
}

lval* builtin_join(lenv* e, lval* a) {
	for (int i = 0; i < a->count; i++) {
		LASSERT_TYPE("join", a, i, LVAL_QEXPR);
	}

	lval* x = lval_pop(a, 0);

	while (a->count) {
		x = lval_join(x, lval_pop(a, 0));
	}

	lval_del(a);
	return x;
}

/* Takes a Value and a Q-Expression and appends it to the front */
lval* builtin_cons(lenv* e, lval* a) {
	LASSERT_NOA("cons", a, 2);
	LASSERT_TYPE("cons", a, 0, LVAL_NUM);
	LASSERT_TYPE("cons", a, 1, LVAL_QEXPR);
	
	lval* value = lval_pop(a, 0);
	int content = value->num;

	lval* value_qex = lval_qexpr();
	lval_add(value_qex, lval_num(content));
	lval_join(value_qex,lval_pop(a, 0));
	lval_del(value);
	lval_del(a);
	
	return value_qex;
}

lval* builtin_len(lenv* e, lval* a) {
	LASSERT_NOA("len", a, 1);
	LASSERT_TYPE("len", a, 0, LVAL_QEXPR);
	
	lval* length = lval_num(a->cell[0]->count);	

	lval_del(a);
	return length;
}
	 

lval* builtin_init(lenv* e, lval* a) {
	LASSERT_NOA("init", a, 1);
	LASSERT_TYPE("init", a, 0, LVAL_QEXPR);
	LASSERT_ELIST("init", a, 0);
	
	lval* result = lval_qexpr();
	lval_pop(a->cell[0], a->cell[0]->count - 1);
	
	while (a->cell[0]->count > 0){
		lval_add(result, lval_pop(a->cell[0], 0));
	}

	lval_del(a);
	return result;
}

lval* builtin_env(lenv* e, lval* a) {

	for (int i = 0; i < e->count; i++) {
		if (e->vals[i]->type != LVAL_FUN) {
			printf("%s = ", e->syms[i]);
			lval_print(e->vals[i], e);
			printf("\n");
		}
	}

	lval_del(a);
	return lval_sexpr();
}

lval* builtin_fun(lenv* e, lval* a) {
	LASSERT_NOA("fun", a, 2);
	LASSERT_TYPE("fun", a, 0, LVAL_QEXPR);
	LASSERT_TYPE("fun", a, 1, LVAL_QEXPR);
	LASSERT_ELIST("fun", a, 0);
	LASSERT_ELIST("fun", a, 1);

	/* Altering the syntax tree and converting it into builtin_def format */
	lval* func_name_qexpr = lval_qexpr();
	lval* func_name = lval_pop(a->cell[0], 0);
	lval_add(func_name_qexpr, func_name);

	/*fun {<func_name> <var1> <var2>} {<body>} converted to
	def {<func_name>} (\ {<var1> <var2} {<body>}) */

	lval* v = lval_sexpr();
	lval* x = lval_sym("def");
	lval* sym = lval_sym("\\");
	lval* lam = lval_sexpr();

	lval_add(lam, sym);
	lval_add(lam, a->cell[0]);
	lval_add(lam, a->cell[1]);
	
	lval_add(v, x);
	lval_add(v, func_name_qexpr);
	lval_add(v, lval_copy(lam));
	
	/* Returning the evaluated result of def-formatted tree */
	lval_del(a);
	return lval_eval(e, v);
	
}


lval* builtin_var(lenv* e, lval* a , char* func) {
	LASSERT_TYPE(func, a, 0, LVAL_QEXPR);
	
	lval* syms = a->cell[0];
	for (int i = 0; i < syms->count; i++) {
		LASSERT(a, (syms->cell[i]->type == LVAL_SYM),
		"Function '%s' cannot define non-symbol. "
		"Got %s, Expected %s.", func,
		ltype_name(syms->cell[i]->type),
		ltype_name(LVAL_SYM));
	}

	LASSERT(a, (syms->count == a->count-1),
	"Function '%s' passed too many arguments for symbols. "
	"Got %i, Expected %i.", func, syms->count, a->count - 1);
	
	for (int i = 0; i < syms->count; i++) {
		/* If 'def' define in globally. If 'put' define in locally */
		if (strcmp(func, "def") == 0) {
			lenv_def(e, syms->cell[i], a->cell[i+1]);
		}
		
		if (strcmp(func, "=") == 0) {
			lenv_put(e, syms->cell[i], a->cell[i+1]);
		}
	}
	
	lval_del(a);
	return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
	return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
	return builtin_var(e, a, "=");
}


lval* builtin_lambda(lenv* e, lval* a) {
	/* Check Two arguments, each of which are Q-Expressions */
	LASSERT_NOA("\\", a, 2);
	LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
	LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);

	/* Check first Q-Expression contains only Symbols */
	for (int i = 0; i < a->cell[0]->count; i++) {
		LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
		"Cannot define non-symbol. Got %s, Expected %s.",
		ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
	}

	/* Pop first two arguments and pass them to lval_lambda */
	lval* formals = lval_pop(a, 0);
	lval* body = lval_pop(a, 0);
	lval_del(a);

	return lval_lambda(formals, body);
}

lval* builtin_greater(lenv* e, lval* a) {
	/*Check Two arguments*/
	LASSERT_NOA(">", a, 2);
	LASSERT(a, ( a->cell[0]->type == a->cell[1]->type ), 
	"Cannot Compare %s and %s", ltype_name(a->cell[0]->type),
	ltype_name(a->cell[1]->type));
	
	lval* first = lval_pop(a, 0);
	lval* second = lval_pop(a, 0);

	switch (first->type) {
		case LVAL_NUM: if (first->num > second->num) return lval_num(1); else return lval_num(0);
	}


	
	lval_del(first);
	lval_del(second);
	lval_del(a);
	return lval_num(0);
	

}

lval* builtin_lesser(lenv* e, lval* a) {
	/*Check Two arguments*/
	LASSERT_NOA("<", a, 2);
	LASSERT(a, ( a->cell[0]->type == a->cell[1]->type ), 
	"Cannot Compare %s and %s", ltype_name(a->cell[0]->type),
	ltype_name(a->cell[1]->type));
	
	lval* first = lval_pop(a, 0);
	lval* second = lval_pop(a, 0);

	switch (first->type) {
		case LVAL_NUM: if (first->num < second->num) return lval_num(1); else return lval_num(0);
	}


	
	lval_del(first);
	lval_del(second);
	lval_del(a);
	return lval_num(0);
}

lval* builtin_equal(lenv* e, lval* a) {
	/*Check Two arguments*/
	LASSERT_NOA(">", a, 2);
	LASSERT(a, ( a->cell[0]->type == a->cell[1]->type ), 
	"Cannot Compare %s and %s", ltype_name(a->cell[0]->type),
	ltype_name(a->cell[1]->type));
	
	lval* first = lval_pop(a, 0);
	lval* second = lval_pop(a, 0);

	switch (first->type) {
		case LVAL_NUM: if (first->num == second->num) return lval_num(1); else return lval_num(0);

		case LVAL_QEXPR: 
			if (first->count == second->count) {
				for (int i = 0; i < first->count; i++) {
					if(first->cell[i]->type != second->cell[i]->type){
						return lval_num(0);
					}
					lval* x = lval_sexpr();
					lval_add(x, first->cell[i]);
					lval_add(x, second->cell[i]);
					lval* equal_output = builtin_equal(e, x);
					if (equal_output->num == 1){
						lval_del(equal_output);
						continue;
					}
					else{
						return lval_num(0);
					}
					

				}
				
				return lval_num(1);
			}
	}
	
	lval_del(first);
	lval_del(second);
	lval_del(a);
	return lval_num(0);
}

lval* builtin_not_equal(lenv* e, lval* a) {
	lval* eq_answer = builtin_equal(e, a);

	if(eq_answer->num == 1) {
		return lval_num(0);
	}

	lval_del(eq_answer);
	return lval_num(1);

}

lval* builtin_greater_equal(lenv* e, lval* a){
	lval* x = lval_copy(a);
	lval* ans = builtin_greater(e, x)
	;
	if (ans->num == 1){
		lval_del(ans);
		return lval_num(1);
	}

	else {
		lval_del(ans);
		return builtin_equal(e, a);
	}

	
}

lval* builtin_lesser_equal(lenv* e, lval* a){
	lval* x = lval_copy(a);
	lval* ans = builtin_lesser(e, x);
	if (ans->num == 1){
		lval_del(ans);
		return lval_num(1);
	}

	else {
		lval_del(ans);
		return builtin_equal(e, a);
	}

	
}

lval* builtin_if(lenv* e, lval* a) {
	LASSERT_NOA("if", a, 3);

	for (int i = 0; i < a->count; i++) {
		LASSERT_TYPE("if", a, i, LVAL_QEXPR);
	}
	
	lval* condition = lval_pop(a, 0);
	char* comp_op[] = {"<", ">", "<=", ">=", "=="};
	int op_flag = 1;

	for (int i = 0; i < sizeof(comp_op)/sizeof(comp_op[0]); i++) {
		if (strcmp(condition->cell[0]->sym, comp_op[i]) == 0) {
			op_flag = 0;
			break;
		}
	}

	if (op_flag == 1) {
		return lval_err("Cannot use non-comparison operators for condition in 'if' function.");
	}
	
	lval* answer = lval_sexpr();
	lval_add(answer, condition);
	
	
	if (builtin_eval(e, answer)->num == 1) {
		lval* if_true = lval_sexpr();
		lval_add(if_true, lval_sym("eval"));
		lval_add(if_true, lval_pop(a, 0));
		lval_del(a);
		return lval_eval(e, if_true);
	}

	
	lval* if_false = lval_sexpr();
	lval_add(if_false, lval_sym("eval"));
	lval_add(if_false, lval_pop(a, 1));
	lval_del(a);
	return lval_eval(e, if_false);
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
	lval* k = lval_sym(name);
	lval* v = lval_fun(func);
	lenv_put(e, k, v);
	lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {
	/* List Functions */
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);
	lenv_add_builtin(e, "cons", builtin_cons);
	lenv_add_builtin(e, "init", builtin_init);
	lenv_add_builtin(e, "def", builtin_def);
	lenv_add_builtin(e, "fun", builtin_fun);
	lenv_add_builtin(e, "if", builtin_if);
	lenv_add_builtin(e, "len", builtin_len);
	lenv_add_builtin(e, "env", builtin_env);
	lenv_add_builtin(e, "\\", builtin_lambda);
	lenv_add_builtin(e, "=", builtin_put);

	/* Mathematical Functions */
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
	lenv_add_builtin(e, "%", builtin_rem);
	lenv_add_builtin(e, ">", builtin_greater);
	lenv_add_builtin(e, "<", builtin_lesser);
	lenv_add_builtin(e, "==", builtin_equal);
	lenv_add_builtin(e, "!=", builtin_not_equal);
	lenv_add_builtin(e, ">=", builtin_greater_equal);
	lenv_add_builtin(e, "<=", builtin_lesser_equal);
	lenv_add_builtin(e, "max", builtin_max);
	lenv_add_builtin(e, "min", builtin_min);
	
}

int main(int argc, char** argv) {
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr = mpc_new("sexpr");
	mpc_parser_t* Qexpr = mpc_new("qexpr");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Lisps = mpc_new("lisps");

	mpca_lang(MPCA_LANG_DEFAULT,
			" 					     	     \
			number: /-?[0-9]+(\\.[0-9]+)?/; 		     \
			symbol: /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/;	     \
			sexpr: '(' <expr>* ')' ;			     \
			qexpr: '{' <expr>* '}' ;			     \
			expr: <number> | <symbol> | <sexpr> | <qexpr> ;      \
			lisps: /^/ <expr>* /$/; 	     		     \
			",
			Number, Symbol, Sexpr, Qexpr, Expr, Lisps);

	/* Print the version and the exit instructions */
	puts("Lisps Version 0.0.0.0.7");
	puts("Press Ctrl+c to Exit\n");

	lenv* e = lenv_new();
	lenv_add_builtins(e);
	
	/* In a never ending loop */
	while (1) {
		
		/* Output our prompt and also get the input */
		char* input = readline("lisps> ");

		if (strcmp(input, "exit") == 0) {
			break;
		}

		/* Add input to history */
		add_history(input);

		/* Attempt to Parse the user Input */
		mpc_result_t r;

		if (mpc_parse("<stdin>", input, Lisps, &r)) {
			//mpc_ast_print(r.output);
			lval* result = lval_eval(e, lval_read(r.output));
			lval_println(result, e);
			lval_del(result);
			printf("Total Number of nodes: %d\n", number_of_nodes(r.output));
			printf("Number of leaf nodes: %d\n", number_leaf_nodes(r.output));
			printf("Number of branches: %d\n\n", number_of_nodes(r.output) - 1);
			mpc_ast_delete(r.output);
		}

		else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
		
		
	}
	lenv_del(e);
	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lisps);
	return 0;
}
