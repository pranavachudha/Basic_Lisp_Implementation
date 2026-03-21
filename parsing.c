#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpc.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

/* Create Enumberation of Possible lval Types */
enum { LVAL_NUM, LVAL_ERR};

/* Create Enumeration of Possible Error Types for the err field in lval struct */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

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
typedef struct {
	int type;
	long num;
	int err;
}lval;

/* Create a new number type lval */
lval lval_num(long x) {
	lval v;
	v.type = LVAL_NUM;
	v.num = x;
	return v;
}

/* Create a new error type lval */
lval lval_err(int x) {
	lval v;
	v.type = LVAL_ERR;
	v.err = x;
	return v;
}

/* Print an "lval" */
void lval_print(lval v) {
	switch(v.type) {
		/* if the v.type value is a number print it and break out */
		case LVAL_NUM: printf("%li", v.num); break;

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
	if (strcmp(op, "%") == 0) { return lval_num(x.num % y.num); }
	if (strcmp(op, "^") == 0) { return lval_num(pow(x.num, y.num)); } 
	if (strcmp(op, "min") == 0) { return lval_num(MIN(x.num, y.num)); }
	if (strcmp(op, "max") == 0) { return lval_num(MAX(x.num, y.num)); }

	/* Division and Remainder operator are prone to Division by zero error */
	if (strcmp(op, "/") == 0) {

	       	return y.num == 0
			? lval_err(LERR_DIV_ZERO)
			: lval_num(x.num / y.num); 
	}
	if (strcmp(op, "%") == 0) {

	       	return y.num == 0
			? lval_err(LERR_DIV_ZERO)
			: lval_num(x.num % y.num); 
	}

	return lval_err(LERR_BAD_OP);
}



lval eval(mpc_ast_t* t) {
	if (strstr(t->tag, "number")) {
		/* Check if there is some error in conversion */
		errno = 0;
		long x = strtol(t->contents, NULL, 10);
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
	mpc_parser_t* Operator = mpc_new("operator");
	mpc_parser_t* Expr = mpc_new("expr");
	mpc_parser_t* Lisps = mpc_new("lisps");

	mpca_lang(MPCA_LANG_DEFAULT,
			" 					     \
			number: /-?[0-9]+/; 			     \
			operator: '+' | '-' | '*' | '/' | '%' | '^' | \"min\" | \"max\"; 	     \
			expr: <number> | '(' <operator> <expr>+ ')'; \
			lisps: /^/ <operator> <expr>+ /$/; 	     \
			",
			Number, Operator, Expr, Lisps);

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

	mpc_cleanup(4, Number, Operator, Expr, Lisps);
	return 0;
}
