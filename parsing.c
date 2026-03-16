#include <stdio.h>
#include <stdlib.h>


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
#include "mpc.h"
#endif

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

/* Use Operator string to see which operation to perform */
long eval_op(long x, char* op, long y) {
	if (strcmp(op, "+") == 0) { return x + y; }
	if (strcmp(op, "-") == 0) { return x - y; }
	if (strcmp(op, "*") == 0) { return x * y; }
	if (strcmp(op, "/") == 0) { return x / y; }
	return 0;
}



long eval(mpc_ast_t* t) {
	/* If tagged as number return it directly. */
	if (strstr(t->tag, "number")) {
		return atoi(t->contents);
	}

	/* The operator is always second child. */
	char * op = t->children[1]->contents;

	/* We store the third child in 'x' */
	long x = eval(t->children[2]);

	/* Iterate the remaining children and combining. */
	int i = 3;

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
			number : /-?[0-9]+/; 			     \
			operator: '+' | '-' | '*' | '/'; 	     \
			expr: <number> | '(' <operator> <expr>+ ')'; \
			lisps: /^/ <operator> <expr>+ /$/; 	     \
			",
			Number, Operator, Expr, Lisps);

	/* Print the version and the exit instructions */
	puts("Lisps Version 0.0.0.0.3");
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
			long result = eval(r.output);
			printf("%li\n", result);
			printf("Total Number of nodes: %d\n", number_of_nodes(r.output));
			mpc_ast_delete(r.output);
		} else {
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
		
		
	}

	mpc_cleanup(4, Number, Operator, Expr, Lisps);
	return 0;
}
