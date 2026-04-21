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
void lval_print(lval* v);
lval* lval_eval(lenv* e, lval* v);
lval* lval_take(lval* v, int i);
lval* lval_pop(lval* v, int i);
lval* builtin_op(lenv* e, lval* a, char* op);
lval* builtin(lval* a, char* func);




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
	lbuiltin fun;
	/* Count and Pointer to a list of "lval*" */
	int count;
	lval** cell;
};

struct lenv {
	int count;
	char** syms;
	lval** vals;
};

lenv* lenv_new(void) {
	lenv* e = malloc(sizeof(lenv));
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
	v->fun = func;
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
		case LVAL_FUN: x->fun = v->fun; break;
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
		case LVAL_FUN: break;
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


void lval_expr_print(lval* v, char open, char close) {
	putchar(open);
	for (int i = 0; i < v->count; i++) {
      		lval_print(v->cell[i]);

		if (i != (v->count-1)) {
			putchar(' ');
		}
      	}
	putchar(close);
}



/* Print an "lval" */
void lval_print(lval* v) {
	switch(v->type) {
		/* if the v.type value is a number print it and break out */
		case LVAL_NUM: printf("%.6g", v->num); break;
		case LVAL_ERR: printf("Error: %s", v->err); break;
		case LVAL_SYM: printf("%s", v->sym); break;
		case LVAL_FUN: printf("<function>"); break;
		case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
		case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
	}
}



/* Print an "lval" followed by a newline */
void lval_println(lval* v) { lval_print(v); putchar('\n'); }


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
		lval_del(f); lval_del(v);
		return lval_err("First element is not a function");
	}

	/* Call builtin with operator */
	lval* result = f->fun(e, v);
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

lval* builtin_def(lenv* e, lval* a) {
	LASSERT_TYPE("def", a, 0, LVAL_QEXPR);
	
	/* First argument is symbol list */
	lval* syms = a->cell[0];

	/* Ensure all elements of first list are symbols */
	for (int i = 0; i < syms->count; i++) {
		LASSERT(a, syms->cell[i]->type == LVAL_SYM,
		"Function 'def' cannot define non-symbol");
	}

	/* Check correct number of symbols and values */
	LASSERT(a, syms->count == a->count-1,
	"Function 'def' cannot define incorrect "
	"number of values to symbols");

	/* Assign copies of values to symbols */
	for (int i = 0; i < syms->count; i++) {
		lenv_put(e, syms->cell[i], a->cell[i+1]);
	}

	lval_del(a);
	return lval_sexpr();
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
	lenv_add_builtin(e, "len", builtin_len);

	/* Mathematical Functions */
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
	lenv_add_builtin(e, "%", builtin_rem);
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
			lval_println(result);
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
