#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpc.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* Create Enumberation of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR };

/* Create Enumeration of Possible Error Types for the err field in lval struct */
//enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM, LERR_REM_DOUBLE };

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
	v->err = x;
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

void lval_del(lval* v) {
	switch (v->type) {
		/* Do nothing special for number type (since it doesn't use a pointer) */
		case LVAL_NUM: break;

		/* For Err or Sym free the string data */
		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;

		/* If Sexpr then delete all elements inside */
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


/* Print an "lval" */
void lval_print(lval v) {
	switch(v.type) {
		/* if the v.type value is a number print it and break out */
		case LVAL_NUM: printf("%lf", v.num); break;

		/* if the v.type value is an error print the appropriate error */	
		case LVAL_ERR:
			       if(v.err == LERR_DIV_ZERO) {
				       printf("Error: Division by Zero!");
			       }

			       if(v.err == LERR_BAD_OP) {
				       printf("Error: Invalid Operator!");
			       }
			       
			       if(v.err == LERR_BAD_NUM) {
				       printf("Error: Invalid Number!");
			       }

			       if(v.err == LERR_REM_DOUBLE) {
				       printf("Error: Invalid Operands for Remainder Operator");
			       }
			       break;
	}
}

/* Print an "lval" followed by a newline */
void lval_println(lval v) { lval_print(v); putchar('\n'); }


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

		

/* Use Operator string to see which operation to perform */
lval eval_op(lval x, char* op, lval y) {

	/* If either value is an error return it */
	if (x.type == LVAL_ERR) { return x; }
	if (y.type == LVAL_ERR) { return y; }

	if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
	if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
	if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
	if (strcmp(op, "^") == 0) { return lval_num(pow(x.num, y.num)); } 
	if (strcmp(op, "min") == 0) { return lval_num(MIN(x.num, y.num)); }
	if (strcmp(op, "max") == 0) { return lval_num(MAX(x.num, y.num)); }

	/* Division and Remainder operator are prone to Division by zero error */
	if (strcmp(op, "/") == 0) {

	       	return y.num == 0
			? lval_err(LERR_DIV_ZERO)
			: lval_num(x.num / y.num); 
	}

	else if (strcmp(op, "%") == 0) {

	       	if (y.num == 0){
			return lval_err(LERR_DIV_ZERO);
		}

		if ((((int) x.num - x.num) == 0)
				&& (((int) y.num - y.num) == 0)) {
		      return lval_num((int) x.num % (int) y.num);
		}
		
		else {
			return lval_err(LERR_REM_DOUBLE);
		}
				
	}

	return lval_err(LERR_BAD_OP);
}



lval eval(mpc_ast_t* t) {
	if (strstr(t->tag, "number")) {
		/* Check if there is some error in conversion */
		errno = 0;
		double x = strtod(t->contents, NULL);
		return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
	}

       	/* The operator is always second child. */
       	char * op = t->children[1]->contents;
	/* We store the third child in 'x' */
	lval x = eval(t->children[2]);

	/* Iterate the remaining children and combining. */
	int i = 3;

	if ((strcmp(t->children[i]->tag, "regex") == 0 
			|| strcmp(t->children[i]->contents, ")") == 0) 
			&& strcmp(op, "-") == 0) {
		return lval_num(-(x.num));
	}

	while(strstr(t->children[i]->tag, "expr")) {
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}
	
	return x;
}




int main(int argc, char** argv) {
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr = mpc_new("sexpr");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Lisps = mpc_new("lisps");

	mpca_lang(MPCA_LANG_DEFAULT,
			" 					     \
			number: /-?[0-9]+(\\.[0-9]+)?/; 			     \
			symbol: '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\";\
			sexpr: '(' <expr>* ')' ;
			expr: <number> | <symbol> | <sexpr> ; \
			lisps: /^/ <expr>* /$/; 	     \
			",
			Number, Symbol, Sexpr, Expr, Lisps);

	/* Print the version and the exit instructions */
	puts("Lisps Version 0.0.0.0.4");
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
			lval result = eval(r.output);
			lval_println(result);
			printf("Total Number of nodes: %d\n", number_of_nodes(r.output));
			printf("Number of leaf nodes: %d\n", number_leaf_nodes(r.output));
			printf("Number of branches: %d\n", number_of_nodes(r.output) - 1);
			mpc_ast_delete(r.output);
		}

		else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
		
		
	}

	mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lisps);
	return 0;
}
