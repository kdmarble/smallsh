// Author: Keith Marble
// Date: 7/10/2020
// CS344 Assignment 2 smallsh

#define _GNU_SOURCE
#define _POSIX_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>

// Global variable required to toggle background mode on/off in custom signal
// handlers, since custom signal handlers can't accept custom arguments and
// are already global constructs
int backgroundAllowed = 1;

// Global variable required to handle SIGINT. Captures the pid of the child
// process so SIGINT can kill only the child process and not the shell
pid_t spawnPid = -5;

// Function to process the user's input and return as a list of strings
void processInput(char* command[], char userInput[], int* backgroundMode, char inputName[], char outputName[], int processID) {
    int i, j;
    // Strip newline, replace it with null terminator
    for (i = 0; i < 2048; i++) {
        if (userInput[i] == '\n') {
            userInput[i] = '\0';
            break;
        }
    }

    // Return empty string if user inputs empty string
    if (strcmp(userInput, "") == 0) {
        command[0] = strdup("");
        return;
    }

    // Chunk user input to get command/args
    char* token = strtok(userInput, " ");

    // Loop while there's still a valid token
    for (i = 0; token; i++) {
        // If there's a & set backgroundMode to 1/true
        if (strcmp(token, "&") == 0) {
            *backgroundMode = 1;
        }
        // If there's a < then what follows is the input file
        else if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " ");
            strcpy(inputName, token);
        }
        // If there's a > then what follows is the output file
        else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " ");
            strcpy(outputName, token);
        }
        // Else the rest if part of command/args
        else {
            command[i] = strdup(token);

            // Expand $$ to processID
            for (j = 0; command[i][j]; j++) {
                // If the current char is $ and the next char is $
                if (command[i][j] == '$' && command[i][j+1] == '$') {
                    // replace the first char with null term
                    command[i][j] == '\0';
                    // replace this string with the processID
                    snprintf(command[i], 256, "%s%d", command[i], processID);
                }
            }
        }

        // Retoken
        token = strtok(NULL, " ");
    }
}

// Custom SIGINT handler
void handle_SIGINT(int signo) {
    // Kill the processID associated with our child
    kill(spawnPid, 2);

    // Write output message, killed with sig 2
    char* message = "\n terminated by signal 2 \n";
    write(STDOUT_FILENO, message, 28);
    fflush(stdout);
}

// Custom SIGTSTP handler
void handle_SIGTSTP(int signo) {
    // If background mode is enabled, toggle it off and display message
    if (backgroundAllowed == 1) {
        char* message = "\nEntering forground-only mode (& is now ignored)\n";
        // We are using write rather than printf
        write(STDOUT_FILENO, message, 51);
        fflush(stdout);
        backgroundAllowed = 0;
    } else {
        // Else, if it's toggled off, enable it and display message
        char* message = "\nExiting forground-only mode\n";
        // We are using write rather than printf
        write(STDOUT_FILENO, message, 31);
        fflush(stdout);
        backgroundAllowed = 1;
    }
}

int main() {
    // Init variables needed for program
    // Flag variable to keep shell running
    bool userQuit = false;

    // Int variables
    // Counter
    int i;
    // ProcessID
    int processID = (int)getpid();
    // Int holder for exit status number
    int exitStatus = 0;
    // Background mode toggle
    int backgroundMode = 0;

    // Char/string variables
    // Input file string
    char inputFile[256];
    // Output file string
    char outputFile[256];
    // Holder for raw user input
    char userInput[2048];
    // Holder for processed user input, array of pointers to strings
    char* command[512];
    // Init processed user input to null
    for (i = 0; i < 512; i++) {
        command[i] = NULL;
    }

    // Set custom function for SIGINT
    // Init struct to empty
    struct sigaction SIGINT_action = {0};
    // Register custom function as sig handler
    SIGINT_action.sa_handler = handle_SIGINT;
    // Block all catchable signals while custom handler is running
    sigfillset(&SIGINT_action.sa_mask);
    // No flags set
    SIGINT_action.sa_flags = 0;
    // Install sig handler
    sigaction(SIGINT, &SIGINT_action, NULL);

    // Set custom function for SIGTSTP
    // Same steps as above
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = handle_SIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    SIGINT_action.sa_flags = 0;
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // While the user hasn't input exit command
    while (!userQuit) {
        // Write prompt colon
        printf(": ");

        // Get user input
        fgets(userInput, 2048, stdin);

        // Pass userInput to processInput function
        processInput(command, userInput, &backgroundMode, inputFile, outputFile, processID);

        // Ignore comment and blank lines
        if (strncmp("#", command[0], 1) == 0 || strncmp("\0", command[0], 1) == 0) {
            command[0][0] == '\0';
            continue;
        }

        // Built in status command, prints out either exit status or
        // terminating signal of last foreground process
        else if (strcmp("status", command[0]) == 0) {
            // If the process terminated normally
            if (WIFEXITED(exitStatus) != 0) {
                // Print exit status with exit status value
                printf("exit value %d \n", WEXITSTATUS(exitStatus));
            } else {
                // Else, the process terminated, print the terminating signal
                printf("terminated by signal %d \n", WTERMSIG(exitStatus));
            }
        }

        // Built in cd command, changes working directory of shell
        else if (strcmp("cd", command[0]) == 0) {
            // If there was an argument passed with cd
            if (command[1]) {
                // Pass argument to chdir
                chdir(command[1]);
            } else {
                // Else, if there's no argument, cd to home ~
                chdir(getenv("HOME"));
            }
        }

        // Built in exit command, sends sigkill to 
        // current process group
        else if (strcmp("exit", command[0]) == 0) {
            kill(0, SIGTERM);
            userQuit = true;
        }

        // If it's not a built in command, fork/exec it
        else {
            // Structure borrowed from lecture
            // Holders for file descriptors 
            int in, out, result;

            // Create fork
            spawnPid = fork();
            switch (spawnPid) {
                // Error case
                case -1:
                    perror("Error forking process \n");
                    exit(1);
                    break;
                
                // Success case
                case 0:
                    // SIGINT signal set to default actions because it would
                    // be called on child instead of parent now
                    SIGINT_action.sa_handler = SIG_IGN;
                    sigfillset(&SIGINT_action.sa_mask);
                    SIGINT_action.sa_flags = SA_RESTART;
                    sigaction(SIGINT, &SIGINT_action, NULL);

                    // We want our child processes to ignore SIGTSTP, only
                    // our shell should react
                    struct sigaction ignore_action = {0};
                    ignore_action.sa_handler = SIG_IGN;
                    sigaction(SIGTSTP, &ignore_action, NULL);

                    // Redirect input, structure borrowed from lecture
                    // If there's an input file
                    if (strlen(inputFile) > 0) {
                        // Open inputFile
                        in = open(inputFile, O_RDONLY);
                        // If the open call errored out, display error
                        if (in == -1) {
                            printf("cannot open %s for input \n", inputFile);
                            fflush(stdout);
                            exit(1);
                        }

                        // Redirect it
                        result = dup2(in, 0);
                        // If error, display message
                        if (result == -1) {
                            perror("Error redirecting in \n");
                            exit(2);
                        }

                        // Close Input
                        fcntl(in, F_SETFD, FD_CLOEXEC);
                    }

                    // Redirect output
                    // If there's an output file
                    if (strlen(outputFile) > 0) {
                        // Open outputFile
                        // Write only, create the file if there is none
                        // truncate it if there is one, set permissions to
                        // default file permission of 666
                        out = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                        // If error, display message
                        if (in == -1) {
                            printf("cannot open %s for output \n", outputFile);
                            fflush(stdout);
                            exit(1);
                        }

                        // Redirect it
                        result = dup2(out, 1);
                        if (result == -1) {
                            perror("Error redirecting out \n");
                            exit(2);
                        }

                        // Close Output
                        fcntl(out, F_SETFD, FD_CLOEXEC);
                    }

                    // Call exec and pass it command and args
                    if (execvp(command[0], command) < 0) {
                        // If it errors out, display message
                        printf("%s: no such command \n", command[0]);
                        fflush(stdout);
                        exit(1);
                    }

                    break;
                
                // Default parent process case
                default:
                    // Execute in background if backgroundMode inputted and 
                    // toggled ON
                    if (backgroundMode && backgroundAllowed) {
                        // Call nonblocking waitpid
                        pid_t newProcID = waitpid(spawnPid, &exitStatus, WNOHANG);
                        printf("background pid is %d \n", spawnPid);
                        fflush(stdout);
                    } else {
                        // Else run in forground and wait
                        pid_t newProcID = waitpid(spawnPid, &exitStatus, 0);
                    }
                
                // Catch any terminated/ended background processes
                while ((spawnPid = waitpid(-1, &exitStatus, WNOHANG)) > 0) {
                    printf("background pid %d is done: ", spawnPid);
                    fflush(stdout);
                    // If the process terminated normally
                    if (WIFEXITED(exitStatus) != 0) {
                        // Print exit status with exit status value
                        printf("exit value %d \n", WEXITSTATUS(exitStatus));
                        fflush(stdout);
                    } else {
                        // Else, the process terminated, print the terminating signal
                        printf("terminated by signal %d \n", WTERMSIG(exitStatus));
                        fflush(stdout);
                    }
                }
            }
        }

        // Reset our variables for the next input
        for (i = 0; command[i]; i++) {
            command[i] = NULL;
        };
        backgroundMode = 0;
        inputFile[0] = '\0';
        outputFile[0] = '\0';
    }

    return 0;
}