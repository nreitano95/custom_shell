#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>


// Define global variables to store the maximum arguments and maximum length
#define MAX_ARGS 512
#define MAX_LENGTH 2048


// Get user input from command line and store in a character array
void print_command(char* inputFilename, char* outputFilename, int* isBackground, char* temp[], int pid) {

    // Print shell command line prompt
    printf(": ");
    fflush(stdout);

    // Initialize a character array to store user input
    char userInput[MAX_LENGTH];

    // Initialize buffer string for expansion of Variable $$
    char* buf;

    // Get user input from command line
    fgets(userInput, MAX_LENGTH, stdin);

    // Remove first instance of '\n' in order to ignore blank command
    int i = 0;
    while (i < strlen(userInput) && userInput[i] != '\n') {
        i++;
    }
    while (i < strlen(userInput)) {
        userInput[i] = userInput[i + 1];
        i++;
    }

    // If the userInput is an empty array, return the function with no command execution
    if (strcmp(userInput, "") == 0) {
        temp[0] = "";
        return;
    }

    // Use strtok to get substrings of the userInput separated by a space 
    char* substring = strtok(userInput, " ");
    
    // Loop through each substring to separate each command.
    int index = 0;
    while (substring) {
        // If input file command, store substring as inputFilename
        if (strcmp(substring, "<") == 0) {
            substring = strtok(NULL, " ");
            inputFilename = substring;
        }

        // If output file command, store substring as outputFilename
        if (strcmp(substring, ">") == 0) {
            substring = strtok(NULL, " ");
            outputFilename = substring;
        }        

        // If background command, set "isBackground" flag to 1;
        if (strcmp(substring, "&") == 0) {
            *isBackground = 1;
        } 
        // Else, parse the command
        else { 
            temp[index] = strdup(substring);
            for (int j = 0; temp[index][j]; j++) {
                // If "$$" is the substring, replace with "\0"
				if (temp[index][j] == '$' && temp[index][j + 1] == '$') {
					temp[index][j] = '\0';
                    buf = temp[index];
					snprintf(buf, MAX_ARGS, "%s%d", temp[index], pid);
				}
            }
        }   

        index++; 

        // Get the next substring
        substring = strtok(NULL, " ");
    }

}

// Initialize a background_flag variable to 0 
int background_flag = 0;

// Execute user's commands
void run_command(char* inputFilename, char* outputFilename, int* childStatus, int* isBackground, char* temp[], struct sigaction SIGINT_action) {

    // Initialize spawn id variable
    pid_t spawnid = -5;
    
    // Create fork
    spawnid = fork();

    switch (spawnid) {
        
        // There is an error with fork()
        case -1:
            perror("fork() did not execute successfully.");
            exit(1);
            break;
        
        // Open the input/output files and execute the user's command
        case 0:
            // Input redirection
            if (strcmp(inputFilename, "") != 0) {
                // Open the input file
                int inputFD = open(inputFilename, O_RDONLY);
                if (inputFD == -1) {
                    perror("open() failed on input file.");
                    exit(1);
                }
                
                // Redirect to the inputFD
                int result = dup2(inputFD, 0);
                if (result == -1) {
                    perror("dup2 failed");
                    exit(2);
                }

                // Close file descriptor
                fcntl(inputFD, F_SETFD, FD_CLOEXEC);
            }

            // Output redirection
            if (strcmp(outputFilename, "") != 0) {
                // Open the output file
                int outputFD = open(outputFilename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
                if (outputFD == -1) {
                    perror("open() failed on output file.");
                    exit(1);
                }
                
                // Redirect to the outputFD
                int result = dup2(outputFD, 1);
                if (result == -1) {
                    perror("dup2 failed");
                    exit(2);
                }

                // Close file descriptor
                fcntl(outputFD, F_SETFD, FD_CLOEXEC);
            }

            // Handle SIGINT command
            SIGINT_action.sa_handler = SIG_DFL;
            sigaction(SIGINT, &SIGINT_action, NULL);

            // Run exec function to execute the file 
            if (execvp(temp[0], (char* const*)temp)) {
                printf("%s: No such file or directory\n", temp[0]);
                fflush(stdout);
                exit(2);
            }

            break;

        default: 
            
            // Push command to background if not already in background
            if ((*isBackground) && (!background_flag)) {
                pid_t childPid = waitpid(spawnid, childStatus, WNOHANG);
                childPid = (childPid + 1 - 1);
                printf("background PID is %d\n", spawnid);
                fflush(stdout);
            } 
            // Else, run the user's command in the foreground
            else {
                pid_t childPid = waitpid(spawnid, childStatus, 0);
                childPid = (childPid + 1 - 1);
            }


        // If any of the background pid's are returned from waitpid(), print pid and status.
        while ((spawnid = waitpid(-1, childStatus, WNOHANG)) > 0) {
            if (WIFEXITED(*childStatus)) {
                printf("background pid %d is done: exit value %d\n", spawnid, WEXITSTATUS(*childStatus));
                fflush(stdout);
            } else {
                printf("background pid %d is done: terminated by signal %d\n", spawnid, WTERMSIG(*childStatus));
                fflush(stdout);
            }
        }
    }
}        

// Signal handler for SIGINT
void handle_SIGINT(int signo) {
    char* message = "terminated by signal 2\n";
        write(STDOUT_FILENO, message, 22);
        fflush(stdout);
}

// Signal handler for SIGTSTP
void handle_SIGTSTP(int signo) {
    
    // If background_flag is not yet, enter foreground-only mode so that '&' is ignored
    if (background_flag == 0) {
        char* message = "Entering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 49);
        fflush(stdout);
        background_flag = 1;
    } 
    // Else, exit the foregound-only mode so '&' is no longer ignored.
    else {
        char* message = "Exiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 29);
        fflush(stdout);
        background_flag = 0;
    }
}


int main() {
    
    char *userInput[512];
    char inputFilename[512] = "";
    char outputFilename[512] = "";
    int isBackground = 0;
    int pid = getpid();
    int runShell = 1;
    int exitCode = 0;

    // Signal SIGINT
    // Initialize SIGINT_action to struct to be empty
    struct sigaction SIGINT_action = {0};
    
    // Fill out the SIGINT_action struct
    SIGINT_action.sa_handler = SIG_IGN;
    
    // Register handle_SIGINT as the signal handler
    SIGINT_action.sa_handler = handle_SIGINT;

    // Block all signals while SIG_IGN is running
    sigfillset(&SIGINT_action.sa_mask);

    // No flags set
    SIGINT_action.sa_flags = 0;

    // Install our signal handler
    sigaction(SIGINT, &SIGINT_action, NULL);
    
    // Signal SIGSTP
    // Initialize SIGTSTP_action to struct to be empty
    struct sigaction SIGTSTP_action = {0};

    // Register handle_SIGTSTP as the signal handler
    SIGTSTP_action.sa_handler = handle_SIGTSTP;

    // Block all signals while the handler is running
    sigfillset(&SIGTSTP_action.sa_mask);

    // No flags set
    SIGTSTP_action.sa_flags = 0;

    // Install our signal handler
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);
    
    // Loop that runs the shell continuously until an exit command is entered
    while (runShell == 1) {
        // Prompt user for input
        print_command(inputFilename, outputFilename, &isBackground, userInput, pid);

        // Stop shell with 'exit' command
        if (strcmp(userInput[0], "exit") == 0) {
            runShell = 0;
        }

        // If user input is a comment, ignore and continue to re-initialize the variables
        if (userInput[0][0] == '#') {
            continue;
        }

        // If user input is blank, continue to re-initialize the variables
        else if (userInput[0][0] == '\0') {
            continue;
        }

        // Change Directory with 'CD' command
        else if (strcmp(userInput[0], "cd") == 0) {
            if (userInput[1] == NULL) {
                chdir(getenv("HOME"));
            } else if (chdir(userInput[1]) == -1) {
                printf("Directory not found.\n");
                fflush(stdout);
            }
        }

        // Handle request for 'status' command
        else if (strcmp(userInput[0], "status") == 0) {
            if (WIFEXITED(exitCode)) {
		        printf("exit value %d\n", WEXITSTATUS(exitCode));
	        } else {
                printf("terminated by signal %d\n", WTERMSIG(exitCode));
	        }
        }

        // Otherwise, run a given command
        else {
            run_command(inputFilename, outputFilename, &exitCode, &isBackground, userInput, SIGINT_action);
        }

        // Re-initialize the userInput array
        for (int i = 0; userInput[i]; i++) {
            userInput[i] = NULL;
        }

        // Re-initialize the variables 
        inputFilename[0] = '\0';
        outputFilename[0] = '\0';
        isBackground = 0;
    }

    return 0;
}
