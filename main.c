/*-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*\
*) ASSIGNMENT 3: SMALLSH (PORTFOLIO ASSIGNMENT)		*	*	*	*	(*
(* Provide a prompt for running commands			*	DONE	*	*)
*) Handle blank lines and comments					*	DONE	*	(*
(* Provide expansion for the variable $$			*	DONE	*	*)
*) exit, cd, and status built into the shell		*	DONE	*	(*
(* Execute other commands with new processes		*	DONE	*	*)
*) Support input and output redirection				*	FIX 	*	(*
(* Support running commands in background 			*	DONE	*	*)
*) custom handlers for SIGINT and SIGTSTP			*	0/1 	*	(*
(* 	>	>	>	>	STATUS: 7 / 8 	<	<	<	^	*	*	*	*	*)	
\*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>


#define MAX_LENGTH 512
#define MAX_ARGS 2048

// Exit status of most recently executed foreground process
int status = 0;

// Indicates whether background processes are disabled
bool foregroundOnlyMode = false;


int numBGProcesses = 0; // Tracks number of background processes
int BGProcesses[MAX_ARGS]; // Array storing PIDs of background processes

void getArgs(int *argc, char **argv); 		// OBJ 1, 2
void shell(int argc, char **argv); 			// OBJ 4, 5
void handleRedirection(char **argv); 		// OBJ 6
void checkBGProcesses();					// OBJ 7
char *replacePID(const char *string, const char *pid_str); // OBJ 3

int main() {
	// Clear terminal
	printf("\033[H\033[J$ smallsh\n");

	while(1) {
		// Total line input
		char *argList[MAX_ARGS];
	
		// Amount of arguments in line 
		int argCount = 0;

		getArgs(&argCount, argList);

		shell(argCount, argList);

		for (int i = 0; i < argCount; i++) {
			free(argList[i]);
		}
	}
	return 0;	
}

void getArgs(int *argc, char **argv) {
	char input[MAX_LENGTH];
	char *processArg;
	pid_t pid = getpid();
	char pid_str[16];
	snprintf(pid_str, 16, "%d", pid);
	
	do {
		printf(": ");
		fgets(input, MAX_LENGTH, stdin);
		strtok(input, "\n");
	} while(input[0] == '#' || strlen(input) == 1);
	

	// separate each process by a blank line
	processArg = strtok(input, " ");
	while (processArg != NULL) {

		// Determining if "$$" is present and replace with pid
		if (strstr(processArg, "$$") != NULL) {
			argv[*argc] = replacePID(processArg, pid_str);
		} else {
			argv[*argc] = strdup(processArg);
		}

		// Incrememt argument count to rerun while loop
		(*argc)++;
		processArg = strtok(NULL, " ");
	}

	// Set the last argument + 1 to NULL
	argv[*argc] = NULL;
}

void shell(int argc, char **argv) {

	// Check for background command 
	bool background = false;
	if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
		// Set background bool to true and delete trailing "&"
		background = true;
		argv[argc - 1] = NULL;
		argc--;
	}
		

	// "exit" special command check
	if (strcmp(argv[0], "exit") == 0) exit(0);

	// "cd" special command check
	if (strcmp(argv[0], "cd") == 0) {
		if (argc == 1) 
			chdir(getenv("HOME"));
		else if (chdir(argv[1]) != 0) {
			printf("cd: %s: No such file or directory\n", argv[1]);
		}
		return;
	}

	// "status" special command check
	if (strcmp(argv[0], "status") == 0) {
		if (WIFEXITED(status))
			printf("exit status %d\n", WEXITSTATUS(status));
		else if (WIFSIGNALED(status))
			printf("terminated by signal %d\n", WTERMSIG(status));
		return;
	}

	// Create new process by forking "process" to hold PID of the child process
	// Returns 0 in the child process on success, -1 in the parent on failure
	pid_t process = fork();
	if (process == -1) {
		perror("fork failed");
	} else if (process == 0) { // Ran by child process
		handleRedirection(argv);
		// Runs command using argv as the list of arguments
		execvp(argv[0], argv);
		exit(1);
	} else { // Ran by parent process
		if (background && !foregroundOnlyMode) {
			
			printf("background pid is %d\n", process);
			BGProcesses[numBGProcesses++] = process;
		} else {
			// Allow child process to complete and store it's status
			waitpid(process, &status, 0);
		
			if (WIFSIGNALED(status)) {
				printf("terminated by signal %d\n", WTERMSIG(status));
			}
		}
	}

	checkBGProcesses();
}

// FIX
void handleRedirection(char **argv) {
	// For every argument passed in line
	for (int i = 0; argv[i] != NULL; i++) {
		// if current iterated argument is <
		if (strcmp(argv[i], "<") == 0 && argv[i + 1] != NULL) {
			
			// Defining the file descriptor
			int FD = open(argv[i + 1], O_RDONLY);
			
			// ensuring open() succeeded
			if (FD == -1) exit(1);


            // Redirect stdin to the opened file descriptor
            // This allows the command to read input from the specified file
			dup2(FD, STDIN_FILENO);

			// closing file descriptor
			close(FD);

			// Ignoring < now that operation is processed
			argv[i] = NULL;			
		}

		// if current iterated argument is >
		if (strcmp(argv[i], ">") == 0 && argv[i + 1] != NULL) {
			
			// Defining the file descriptor
			int FD = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	
			// ensuring open() succeeded
			if (FD == -1) exit(1);

            // Redirect stdout to the opened file descriptor
            // This allows the command to write output to the specified file
			dup2(FD, STDOUT_FILENO);

			// closing file descriptor
			close(FD);

			// Ignoring > now that operation is processed
			argv[i] = NULL;
		}
	}
}


void checkBGProcesses() {
	// For every background process
	for (int i = 0; i < numBGProcesses; i++) {
		int BGStatus;

		// check status of background process without blocking
		// WNOHANG makes waitpid return 0 instead of waiting
		pid_t result = waitpid(BGProcesses[i], &BGStatus, WNOHANG);

		// waitpid returns a positive value upon finishing
		if (result > 0)

			// Check if the process exited normally
			if (WIFEXITED(BGStatus)) {

				// Print the exit status of the completed BG  process
				printf("background pid %d is done: exit value %d\n",
									 result, WEXITSTATUS(BGStatus));

			// Check if process was terminated by a signal
			} else if (WIFSIGNALED(BGStatus)) {
			
				// Print signal number that terminated the BG process
				printf("background pid %d is done: terminated by signal %d\n",
												  result, WTERMSIG(BGStatus));
			}
			
			// Remove completed process from list of BG processes
			BGProcesses[i] = BGProcesses[numBGProcesses--];

			// Recheck index in for loop now that numBGProcesses is decremented
			i--;
	}
}

char *replacePID(const char *string, const char *pid_str) {
	char newString[MAX_LENGTH] = "";
	const char *startPosition = string;
	char *dollarPosition;

	// Search for the first occurrence of "$$" starting from startPosition
	// dollarPosition holds the pointer to the first occurence of $$
	while ((dollarPosition = strstr(startPosition, "$$")) != NULL) {

		// Copying text before "$$"
		strncat(newString, startPosition, dollarPosition - startPosition);
	
		// Append the process ID with the new string
		strcat(newString, pid_str);

		// Move the startPosition past the "$$"
		startPosition = dollarPosition + 2;
	}
	
	// Append the rest of the string after the last "$$"	
	strcat(newString, startPosition);

	// Copy new string onto old argument
	return strdup(newString);
}
