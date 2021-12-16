// Author Name: Cheng Ying Wu
// Date: 11/1/2021
// Course: CS 344 - Operating Systems I 
// Project: 3 (Assignment 3: smallsh (Portfolio Assignment))
// Description: Write my own shell in C. This shell will implement a subset of features of well-known shells, such as bash

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/wait.h>
#include <signal.h>
#include <ctype.h>

// Global Variables
int         closeBackground = 0;     // 0: Allow Background Commands; 1: Close Background Commands
static char expandedStr[256];        // Set up an expanded string
char        *commands[512];          // Use a string array to store each commands in the user input

/*
* Check whether the input string is all spaces
*/
int isBlank(char *userInput)
{
    int allSpace = 1;     // 1: all spaces; 0: not all spaces
    int inputLen = strlen(userInput);

    // Use isspace() to check each characters in the string
    for (int i = 0; i < inputLen; i++)
    {
        // isspace(): Return a zero if this character is not a white-space character
        if (isspace(userInput[i]) == 0)
        {
            // 0: not all spaces
            allSpace = 0;
        }
    }

    // Return the result
    return allSpace;
}

// Function used to clear the values in the expanded string
void clearVal()
{
    // Clear the values stored in the expanded string
    for (int a = 0; a < 256; a++)
    {
        // Assign NULL to it
        expandedStr[a] = '\0';
    }

}

// Function used to clear the values in the command
void clearCommand()
{
    // Clear the values stored in the command array
    for (int x = 0; x < 512; x++)
    {
        // Assign NULL to it
        commands[x] = NULL;
    }

}

/*
* 3. Expansion of Variable $$
* Expand any instance of "$$" in a command into the process ID of the smallsh itself
*/
char* expandVar(char *instance, int shPid)
{
    // Copy the value of the input string
    char receivedStr[256];
    sprintf(receivedStr, "%s", instance);
    int  stringLen = strlen(receivedStr);
    
    int  expand = 0;        // Used to indicate whether the input string is expanded
    int  lastIndex = 0;     // Used to track the last index of the expanded string
    
    // Transfer smallsh pid value to a string
    char pidStr[256];
    sprintf(pidStr, "%d", shPid);
    int  pidLen = strlen(pidStr);

    // Clear up the values
    clearVal();

    // Iterate through each character in the input string
    for (int i = 0; i < stringLen; i++)
    {
        // If "$$" appears, expand the string with the process ID of the smallsh itself
        if (receivedStr[i] == '$' && receivedStr[i+1] == '$')
        {
            // Fill the string with the process ID of the smallsh itself
            for (int x = 0; x < pidLen; x++)
            {
                expandedStr[lastIndex] = pidStr[x];
                lastIndex++;
            }
            i++;           // Skip the next character
            expand = 1;    // Indicate it is expanded
        }
        else
        {
            // Fill the string with the original character
            expandedStr[lastIndex] = receivedStr[i];
            lastIndex++;
        }
    }

    // Set up a string with the value of "None"
    char *noExpanded = "None";

    // Return different values according to the expansion indicator
    if (expand == 1)
    {
        // If the string is expanded, then return the expanded string
        return expandedStr;
    }
    else
    {
        // Otherwise, return "None"
        return noExpanded;
    }
}

/*
* 8. Signals SIGTSTP
*/
/* Citation:
* Based on the example program used within Exploration: Signal Handling API - Example: Custom Handlers for SIGINT, SIGUSR2, and Ignoring SIGTERM, etc.
* Source URL: https://canvas.oregonstate.edu/courses/1830250/pages/exploration-signal-handling-api?module_item_id=21468881
* Reason: The sample code shows how to use the signal handling API to customize the handler for signals
*/
// Handler for SIGTSTP
void handle_SIGTSTP(int signo)
{
	char* enterMessage = "\nEntering foreground-only mode (& is now ignored)\n";
    char* exitMessage = "\nExiting foreground-only mode\n";
	
    if (closeBackground == 0)
    {
        // When first receive SIGTSTP
        // Display an informative message
        write(STDOUT_FILENO, enterMessage, 50);
        fflush(stdout);

        // Indicate that the background mode is closed
        closeBackground = 1;
    }
    else
    {
        // When receive SIGTSTP again
        // Display another informative message
        write(STDOUT_FILENO, exitMessage, 30);
        fflush(stdout);

        // Reopen the background mode
        closeBackground = 0;
    }
	
}


// ---------- Main Function ----------

int main(void)
{
	// Set a variable to control whether exit the program
    int   exitsh = 1;           // 0: exit my shell
    
    // Declare some variables
    char  userInput[2049];      // Must support command lines with a maximum length of 2048 characters, and a maximum of 512 arguments
    // char  currDir[256];      // Used to check the result of "cd" command
    int   childStatus;
    pid_t spawnPid;             
    int   index;
    char  *retVal;              // Store the result returned by expandVar()
    int   result;
    int   backCommand;          // 0: Foreground Commands; 1: Background Commands

    // Redirection Indicators and variables
    int   inRedirection;        // 0: no "<" in the command; 1: "<" exists
    int   outRedirection;       // 0: no ">" in the command; 1: ">" exists
    char  inFile[256];          // Store the name of the input file
    char  outFile[256];         // Store the name of the output file

    // Get the process ID of smallsh by using getpid()
    pid_t smallshPid = getpid();


    /*
    * 8. Signals SIGINT & SIGTSTP
    * a. SIGINT  (CTRL-C): Parent and background child processes ignore SIGINT, but a foreground child process must terminate itself when it receives SIGINT
    * b. SIGTSTP (CTRL-Z): Foreground and background child processes ignore SIGINT, and the Parent process controls the status of the background mode
    */
    /* Citation:
    * Based on the example program used within Exploration: Signal Handling API - Example: Custom Handlers for SIGINT, SIGUSR2, and Ignoring SIGTERM, etc.
    * Source URL: https://canvas.oregonstate.edu/courses/1830250/pages/exploration-signal-handling-api?module_item_id=21468881
    * Reason: This sample code illustrates how to use the signal handling API
    */
    // Initialize SIGINT_action & SIGTSTP_action struct to be empty
	struct sigaction SIGINT_action = {{0}}, SIGTSTP_action = {{0}}; 

    /* My shell must ignore SIGINT */
    // Fill out the SIGINT_action struct
	// Register SIG_IGN as the signal handler
	SIGINT_action.sa_handler = SIG_IGN;
	sigaction(SIGINT, &SIGINT_action, NULL);

    /* My shell uses SIGTSTP to control the background mode */
    // Fill out the SIGTSTP_action struct
	// Register SIGTSTP_action as the signal handler
	SIGTSTP_action.sa_handler = handle_SIGTSTP;
	// Block all catchable signals while handle_SIGTSTP is running
	sigfillset(&SIGTSTP_action.sa_mask);
	// No flags set
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    // Keep enabling the user to input commands until the user chooses to exit
    while (exitsh != 0)
    {
        
        // Clear up the command
        clearCommand();
        
        // Use a signal handler to immediately wait() (when the value of pid is -1, it works similar to wait()) for child processes that terminate
        // The time to print out when these background processes have completed is just BEFORE command line access
        if ((spawnPid = waitpid(-1, &childStatus, WNOHANG)) > 0)
        {
            // When a background process terminates, a message showing the process id and exit status will be printed
            printf("background pid %d is done: ", spawnPid);
            fflush(stdout);
            /* Citation:
            * Based on the example program used within Exploration: Process API - Monitoring Child Processes - Interpreting the Termination Status
            * Source URL: https://canvas.oregonstate.edu/courses/1830250/pages/exploration-process-api-monitoring-child-processes?module_item_id=21468873
            * Reason: This example program is used to interpret the Termination Status
            */ 
            // Exit normally with status
            if (WIFEXITED(childStatus))
            {
                // If this command is run before any foreground command is run, then it should simply return the exit status 0
                printf("exit value %d\n", WEXITSTATUS(childStatus));
                fflush(stdout);
            }
            // Terminate due to signal		    
            else
            {
                printf("terminated by signal %d\n", WTERMSIG(childStatus));
                fflush(stdout);
            }
        }
        
        // Get the user's input
        printf(": ");
        fflush(stdout);
        /* Citation:
        * Source URL: https://www.geeksforgeeks.org/taking-string-input-space-c-3-different-methods/
        * Reason: Cannot simply use scanf() since it only works correctly with strings without spaces
        * Instead, I use fgets(), but it needs to remove the newline
        */ 
        // scanf("%[^\n]%*c", userInput);
        fgets(userInput, 2049, stdin);
        strtok(userInput, "\n");    // Remove the newline
        fflush(stdout);
        // printf("%s\n", userInput);
        // printf("Length: %zu\n", strlen(userInput));

        /* 
        * 4. Built-in Commands 
        * a. exit: The exit command exits my shell
        * b. cd: The cd command changes the working directory
        * c. status: The status command prints out either the exit status or the terminating signal of the last foreground process ran
        */
        // If the user enters "exit", then exit the shell
        if (strncmp(userInput, "exit", 4) == 0)
        {
            // Kill any other processes or jobs
            signal(SIGQUIT, SIG_IGN);
            kill(0, SIGQUIT);
            // Exit the shell
            exitsh = 0;
        }
        // Use strncmp() to check the first two characters is whether "cd"
        else if (strncmp(userInput, "cd", 2) == 0)
        {
            // Check the length of the input to see whether it has an argument
            if (strlen(userInput) == 2 || strcmp(userInput, "cd &") == 0)
            {
                // By itself - with no arguments: Use suggested chdir()
                // Also, ignore the & option
                chdir("/nfs/stak/users");    // HOME directory: /nfs/stak/users

                // Check the result
                // getcwd(currDir, sizeof(currDir));
                // printf("Current Directory: %s\n", currDir);
            }
            // With one argument: The path of a directory to change to
            else
            {
                // Use strtok() to get the path 
                /* Citation:
                * Based on the example program used within Exploration: Strings - Basic String Functions - strtok
                * Source URL: https://replit.com/@cs344/34strtokc
                */ 
                // In the 1st call, we specify the string to tokenize. Delimiters is space
                char* token = strtok(userInput, " ");
                // 1st token will be "cd"   
                // printf("1st token = %s\n", token);    
                // 2nd token will be the specified path by the user
                token = strtok(NULL, " ");
                // printf("Path = %s!\n", token);

                /* 3. Expansion of Variable $$ */
                // Call expandVar() to check whether it has an instance of "$$" in a command
                retVal = expandVar(token, smallshPid);

                // If yes, expand it (If no, the returned value will be "None")
                if (strcmp(retVal, "None") != 0)
                {
                    // Reassign the new expanded value to the argument
                    token = retVal;
                }

                // Use suggested chdir() to change the directory (Also, handle the case that the path is incorrect)
                if (chdir(token) != 0)
                {
                    perror("Error");
                } 

                // Check the result
                // getcwd(currDir, sizeof(currDir));
                // printf("Current Directory: %s\n", currDir);
            }
        }
        // status: prints out either the exit status or the terminating signal of the last foreground process ran by my shell
        else if (strncmp(userInput, "status", 6) == 0)
        {
            /* Citation:
            * Based on the example program used within Exploration: Process API - Monitoring Child Processes - Interpreting the Termination Status
            * Source URL: https://canvas.oregonstate.edu/courses/1830250/pages/exploration-process-api-monitoring-child-processes?module_item_id=21468873
            * Reason: This example program is used to interpret the Termination Status
            */ 
            // Exit normally with status
            if (WIFEXITED(childStatus))
            {
                // If this command is run before any foreground command is run, then it should simply return the exit status 0
                printf("exit value %d\n", WEXITSTATUS(childStatus));
                fflush(stdout);
            }
            // Terminate due to signal		    
            else
            {
                printf("terminated by signal %d\n", WTERMSIG(childStatus));
                fflush(stdout);
            }
        }
        
        /* 
        * 2. Comments & Blank Lines 
        * a. comments: Any line that begins with the # character is a comment line and should be ignored
        * b. blank line: A blank line (one without any commands) should also do nothing
        */
        // Use strncmp() to check the first character is whether "#"
        else if (strncmp(userInput, "#", 1) == 0)
        {
            // If yes: Do nothing, just continue
            continue;
        }
        // Call isBlank() to check whether it is the input without any commands
        else if (isBlank(userInput) == 1)
        {
            // If yes: Do nothing, just continue
            continue;
        }

        /* 
        * 5. Executing Other Commands
        * Execute any commands other than the 3 built-in command by using fork(), exec(), and waitpid()
        */
        else
        {
            /* Citation:
            * Based on the example program used within Exploration: Strings - Basic String Functions - strtok
            * Source URL: https://replit.com/@cs344/34strtokc
            * Reason: Use strtok() to get the main command and each arguments
            */ 
            // In the 1st call, we specify the string to tokenize. Delimiters is space
            commands[0] = strtok(userInput, " ");    // 1st token will be the command to be executed
            // printf("Command: %s\n", commands[0]);    // Check the command
            index = 0;

            // Initialize the redirection indicators to 0 (no "<" & ">" in the command)
            inRedirection = 0;
            outRedirection = 0; 

            // Assume the command is foreground
            backCommand = 0;
    
            // Use a while loop to store each arguments to the string array (commands)
            while (commands[index] != NULL)
            {
                // Increase the index accordingly
                index++;
                // Assume that a command is made up of words separated by spaces
                commands[index] = strtok(NULL, " ");

                // Keep checking whether the command has special symbols "<", ">", and "$"
                if (commands[index] != NULL)
                {
                    // printf("Checking: %s\n", commands[index]);
                    // Check Input & Output Redirection
                    if (strcmp(commands[index], "<") == 0)
                    {
                        // "<" exists
                        inRedirection = 1;                      // Indicate it
                        commands[index] = strtok(NULL, " ");    // commands[index] = input file
                        strcpy(inFile, commands[index]);        // Copy its value
                        index--;                                // Maintain the current index value in the next loop
                        continue;
                    }
                    if (strcmp(commands[index], ">") == 0)
                    {
                        // ">" exists
                        outRedirection = 1;                     // Indicate it
                        commands[index] = strtok(NULL, " ");    // commands[index] = output file
                        strcpy(outFile, commands[index]);       // Copy its value
                        index--;                                // Maintain the current index value in the next loop
                        continue;
                    }

                    /* 3. Expansion of Variable $$ */
                    // Call expandVar() to check whether it has an instance of "$$" in a command
                    retVal = expandVar(commands[index], smallshPid);

                    // If yes, expand it (If no, the returned value will be "None")
                    if (strcmp(retVal, "None") != 0)
                    {
                        // Reassign the new expanded value to the argument
                        commands[index] = retVal;
                    }
                }
                // Check the arguments
                // printf("Arguments: %s\n", commands[index]);
            }
            
            /* 
            * Finally, check "&"
            * Since if the command is to be executed in the background, the last word must be &, so we check it lastly
            * The value of the current index minus 1 is the index of the last argument
            * If the "&" is detected, flag it as the background command, and remove it from the command
            * Also, according to the SIGTSTP signal to decide whether ignore it or not
            */
            if (strcmp(commands[index - 1], "&") == 0)
            {
                // If the background mode is not closed
                if (closeBackground == 0)
                {
                    // Flag it as the Background Commands
                    // printf("Background Commands\n");
                    backCommand = 1;
                }

                // Remove this argument from the command
                commands[index - 1] = NULL;
            }

            /* Execute the command */
            
            /* Citation:
            * Based on the example program used within Exploration: Process API - Executing a New Program - Using exec() with fork() 
            * Source URL: https://canvas.oregonstate.edu/courses/1830250/pages/exploration-process-api-executing-a-new-program?module_item_id=21468874
            * Reason: It executes the command by using fork(), exec(), and waitpid()
            */ 
            // Fork a new process (child)
	        spawnPid = fork();

	        switch(spawnPid)
            {
	        case -1:
		        perror("fork()\n");
		        exit(1);
		        break;
	        case 0:
		        /* In the child process */

                /*
                * 8. Signals SIGINT
                * A child running as a foreground process must terminate itself when it receives SIGINT
                * Any children running as background processes must ignore SIGINT
                */
                // Turn back the default signal handling for SIGINT when running as a foreground process
                if (backCommand == 0)
                {
                    SIGINT_action.sa_handler = SIG_DFL;
	                sigaction(SIGINT, &SIGINT_action, NULL);
                }

                /*
                * 6. Input & Output Redirection
                * When redirection symbols are detected, the program will open the file, redirect the input or output, and finally close the file
                */
                /* Citation:
                * Based on the example program used within Exploration: Processes and I/O - Redirecting Input and Output (Example)
                * Source URL: https://canvas.oregonstate.edu/courses/1830250/pages/exploration-processes-and-i-slash-o?module_item_id=21468882
                * Reason: The example code redirect standard input to read from a file and standard out to write to a file by using dup2() and exec(), as required
                * Also, it opens the file, as described in the specification 
                * The shell should respect the input and output redirection operators for a command regardless of background or foreground
                */ 
                // Input Redirection
                if (inRedirection != 0)
                {
                    // Open source file (for reading only)
                    int sourceFD = open(inFile, O_RDONLY);
                    // The case that cannot open the file for reading
	                if (sourceFD == -1) { 
                        // Print an error message and set the exit status to 1
		                printf("cannot open %s for input\n", inFile);
                        fflush(stdout);
		                exit(1); 
	                }

	                // Redirect stdin to source file
	                result = dup2(sourceFD, 0);
	                if (result == -1) { 
		                perror("source dup2()"); 
                        // fflush(stdout);
		                exit(2); 
	                }

                    // Close File Descriptor On Exec
                    fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
                }
                // Output Redirection
                if (outRedirection != 0)
                {
                    // Open target file (for writing only; truncate if it already exists; create if it does not exist)
	                int targetFD = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    // The case that cannot open the output file
	                if (targetFD == -1) { 
                        // Print an error message and set the exit status to 1
		                printf("cannot open %s for output\n", outFile);
                        fflush(stdout); 
		                exit(1); 
	                }
  
	                // Redirect stdout to target file
	                result = dup2(targetFD, 1);
	                if (result == -1) { 
		                perror("target dup2()"); 
		                exit(2); 
	                }

                    // Close File Descriptor On Exec
                    fcntl(targetFD, F_SETFD, FD_CLOEXEC);
                }

                /* /dev/null Implementation 
                * If the user doesn't redirect the standard input for a background command, then standard input should be redirected to /dev/null
                * If the user doesn't redirect the standard output for a background command, then standard output should be redirected to /dev/null
                */
                // No Input Redirection for a background command
                if (inRedirection == 0 && backCommand == 1)
                {
                    // Open source file (for reading only)
                    int sourceFD = open("/dev/null", O_RDONLY);    // Redirect it to /dev/null
                    // The case that cannot open the file for reading
	                if (sourceFD == -1) { 
                        // Print an error message and set the exit status to 1
		                printf("cannot open %s for input\n", inFile);
                        fflush(stdout);
		                exit(1); 
	                }

	                // Redirect stdin to source file
	                result = dup2(sourceFD, 0);
	                if (result == -1) { 
		                perror("source dup2()"); 
                        // fflush(stdout);
		                exit(2); 
	                }

                    // Close File Descriptor On Exec
                    fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
                }
                // No Output Redirection for a background command
                if (outRedirection == 0 && backCommand == 1)
                {
                    // Open target file (for writing only; truncate if it already exists; create if it does not exist)
	                int targetFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);    // Redirect it to /dev/null
                    // The case that cannot open the output file
	                if (targetFD == -1) { 
                        // Print an error message and set the exit status to 1
		                printf("cannot open %s for output\n", outFile);
                        fflush(stdout); 
		                exit(1); 
	                }
  
	                // Redirect stdout to target file
	                result = dup2(targetFD, 1);
	                if (result == -1) { 
		                perror("target dup2()"); 
		                exit(2); 
	                }

                    // Close File Descriptor On Exec
                    fcntl(targetFD, F_SETFD, FD_CLOEXEC);
                }

		        // Use suggested execvp() since it takes an array of strings terminated with a NULL element as arguments 
		        execvp(commands[0], commands);
		        // If exec() is told to execute something that it cannot do, it will fail and return why
		        fprintf(stderr, "%s: ", commands[0]);
                perror("");
                fflush(stdout);
                // In this case, set the exit status to 1
                exit(1);
                break;
	        default:
		        /* In the parent process */
		        
                /* 
                * 7. Executing Commands in Foreground & Background 
                */
                if (backCommand == 1)
                {
                    /* Background Commands (Any non built-in command with an & at the end)
                    * The shell must not wait for such a command to complete
                    * Non-blocking Wait Using WNOHANG (In Exploration: Process API - Monitoring Child Processes)
                    */
                    // WNOHANG specified. If the child hasn't terminated, waitpid will immediately return with value 0
		            waitpid(spawnPid, &childStatus, WNOHANG);
                    // Print the process id of a background process when it begins
                    printf("background pid is %d\n", spawnPid);
                    fflush(stdout);
                }
                else
                {
                    /* Foreground Commands (Any command without an & at the end)
                    * Wait for child's termination
                    * A child process must terminate after running a command (whether the command is successful or it fails)
                    * For a foreground command, it is recommend to have the parent simply call waitpid() on the child, while it waits
                    */
		            spawnPid = waitpid(spawnPid, &childStatus, 0);

                    // If a child foreground process is killed by a signal, print out the number of the signal that killed it's foreground child process
                    // Terminate due to signal
                    if (WIFSIGNALED(childStatus))
                    {
                        // If WIFSIGNALED returned true, WTERMSIG will return the signal number that caused the child to terminate
                        printf("terminated by signal %d\n", WTERMSIG(childStatus));
                        fflush(stdout);
                    }
                }
    
	        } 
            
        }

    }

    // Exit from the program
    return EXIT_SUCCESS;
	
}
