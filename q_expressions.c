#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define LASSERT(args, cond, err) \
	if (!(cond)) { lval_del(args); return lval_err(err); }

/* Create Enumberation of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };


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

/* Declare New lval(lisp value) struct */
typedef struct lval{
	int type;
	double num;
	/*Error and symbol types have some string data */
	char* err;
	char* sym;
	/* Count and Pointer to a list of "lval*" */
	int count;
	struct lval** cell;
}lval;


/* Construct a pointer to a new Number lval */
lval* lval_num(double x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

/* Construct a pointer to a new Error lval */
lval* lval_err(char* x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(x) + 1);
	strcpy(v->err, x);
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


void lval_del(lval* v) {
	switch (v->type) {
		/* Do nothing special for number type (since it doesn't use a pointer) */
		case LVAL_NUM: break;

		/* For Err or Sym free the string data */
		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;

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

void lval_print(lval* v);

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

		

lval* lval_eval(lval* v);
lval* lval_take(lval* v, int i);
lval* lval_pop(lval* v, int i);
lval* builtin_op(lval* a, char* op);
lval* builtin(lval* a, char* func);

lval* lval_eval_sexpr(lval* v){
	/* Evaluate Children */
	for (int i = 0; i < v->count; i++){
		v->cell[i] = lval_eval(v->cell[i]);
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
	if (f->type != LVAL_SYM) {
		lval_del(f); lval_del(v);
		return lval_err("S-expression Does not start with Symbol!");
	}

	/* Call builtin with operator */
	lval* result = builtin(v, f->sym);
	lval_del(f);
	return result;
}

lval* lval_eval(lval* v) {
	/* Evaluate S-expressions */
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(v); }
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

lval* builtin_op(lval* a, char* op) {

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

lval* builtin_head(lval* a) {
	/* Check Error Conditions */
	LASSERT(a, a->count == 1,
	 "Function 'head' passed too many arguments!")

	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
	 "Function 'head' passed incorrect type!");

	LASSERT(a, a->cell[0]->count != 0,
	 "Function 'head' passed {}!");

	lval* v = lval_take(a, 0);

	/* Delete all elements that are not head and return */
	while (v->count > 1) { lval_del(lval_pop(v, 1)); }
	return v;
}

lval* builtin_tail(lval* a) {
	/* Check Error Conditions */
	LASSERT(a, a->count == 1,
	 "Function 'tail' passed too many arguments!")

	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
	 "Function 'tail' passed incorrect type!");

	LASSERT(a, a->cell[0]->count != 0,
	 "Function 'tail' passed {}!");

	lval* v = lval_take(a, 0);

	lval_del(lval_pop(v, 0));
	return v;
}

lval* builtin_list(lval* a) {
	a->type = LVAL_QEXPR;
	return a;
}

lval* builtin_eval(lval* a) {
	LASSERT(a, a->count == 1,
	 "Function 'eval' passed too many arguments!");

	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
	 "Function 'eval' passed incorrect type!");

	lval* x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(x);
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

lval* builtin_join(lval* a) {
	for (int i = 0; i < a->count; i++) {
		LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
      "Function 'join' passed incorrect type.");
	}

	lval* x = lval_pop(a, 0);

	while (a->count) {
		x = lval_join(x, lval_pop(a, 0));
	}

	lval_del(a);
	return x;
}

lval* builtin(lval* a, char* func) {
	if (strcmp("list", func) == 0) { return builtin_list(a); }
	if (strcmp("head", func) == 0) { return builtin_head(a); }
	if (strcmp("tail", func) == 0) { return builtin_tail(a); }
	if (strcmp("join", func) == 0) { return builtin_join(a); }
	if (strcmp("eval", func) == 0) { return builtin_eval(a); }
	if (strstr("+-/%*minmax", func)) { return builtin_op(a, func); }
	lval_del(a);
	return lval_err("Unknown Function!");
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
			symbol: '+' | '-' | '*' | '/' | '%' | '^' 	     \
	   		| \"min\" | \"max\" | \"list\" | \"head\" | \"tail\" \
			| \"join\" | \"eval\";				     \
			sexpr: '(' <expr>* ')' ;			     \
			qexpr: '{' <expr>* '}' ;			     \
			expr: <number> | <symbol> | <sexpr> | <qexpr> ; 		     \
			lisps: /^/ <expr>* /$/; 	     		     \
			",
			Number, Symbol, Sexpr, Qexpr, Expr, Lisps);

	/* Print the version and the exit instructions */
	puts("Lisps Version 0.0.0.0.6");
	puts("Press Ctrl+c to Exit\n");

	
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
			lval* result = lval_eval(lval_read(r.output));
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

	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lisps);
	return 0;
}
