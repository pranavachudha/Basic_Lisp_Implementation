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
#endif

int main(int argc, char** argv) {

	/* Print the version and the exit instructions */
	puts("Lisps Version 0.0.0.0.1");
	puts("Press Ctrl+c to Exit\n");

	/* In a never ending loop */
	while (1) {

		/* Output our prompt and also get the input */
		char* input = readline("lisps> ");

		/* Add input to history */
		add_history(input);

		/* Echo input back to user */
		printf("Entered Text:  %s\n", input);

		/* Free retrieved input */
	}

	return 0;
}

