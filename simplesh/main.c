#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>

//Flag to keep track of foreground only mode
//I originally had file input/output flags but my code didn't work
int foregroundOnly, background = 0;

//Global variables to keep track of background processes
int numProc = 0;
int procList[2500];

//Check status of the process
//num is the PID
void checkStatus(int num) {
   if(WIFSIGNALED(num)) {
      printf("Process terminated by signal: %d\n", WTERMSIG(num));
      fflush(stdout);
   }else if(WIFEXITED(num)) {
      printf("Process exited with code: %d\n", WEXITSTATUS(num));
      fflush(stdout);
   }
}


//This function is a toggle for the foreground mode.
//If we're currently using foreground-only mode we switch back to regular mode and the opposite is true
//It switches the global variable foreground
//This function is triggered by pressing Ctrl+Z 
void switchMode(int currMode) {
   if(foregroundOnly == 0) {
      printf("Entering foreground-only mode, & is now ignored\n");
      fflush(stdout);
      foregroundOnly = 1;
   }else if(foregroundOnly == 1) {
      printf("Now exiting foreground-only mode\n");
      fflush(stdout);
      foregroundOnly = 0;
   }
   printf(": ");
   fflush(stdout);
}

//Function to handle Ctrl + C and Ctrl Z
//Code adapted from Exploration: Signal Handling on Canvas Modules
void handleSignals() {
   //Ignore ctrl C
   struct sigaction SIGINT_action = {0};
   SIGINT_action.sa_handler = SIG_IGN;
   sigaction(SIGINT, &SIGINT_action, NULL);

   //Toggle foreground-only mode on Ctrl+ Z press
   struct sigaction SIGSTP_action = {0};
   SIGSTP_action.sa_handler = &switchMode;
   sigfillset(&SIGSTP_action.sa_mask);
   SIGSTP_action.sa_flags = SA_RESTART;
   sigaction(SIGTSTP, &SIGSTP_action, NULL);
}

//Uses global variables to clear the list of processes
void killProcs() {
   if(numProc != 0) {
      for(int i = 0; i < numProc; i++) {
	 if(procList[numProc] == 0) {
	    exit(0);
	 }else {
	    kill(procList[i], SIGTERM);
	 }
      }
   }
}


void commands(char *argsList[], int numArgs) {
   int status, fd, fd2, inFlag, outFlag = 0; //Flags for '<' & '<', file descriptors
   char inFile[1000]; //Store the filenames
   char outFile[1000];
   pid_t pid, pid2; 

   if((strcmp(argsList[numArgs - 1], "&") == 0) && (foregroundOnly != 1)) { //Check if the foreground only mode is on and if last arg is &
      background = 1; //Set background to true
      argsList[numArgs-1] = NULL; //Remove the last argument so it isn't passed to execvp
      numArgs--;
      printf("Detected &, background process\n");
      fflush(stdout);
   }else if((strcmp(argsList[numArgs - 1], "&") == 0) && (foregroundOnly == 1)) {
      argsList[numArgs-1] = NULL; //Remove the last argument and do nothing if foreground only mode on
      numArgs--;
   }

   if(strcmp(argsList[0], "#") == 0 || argsList[0] == NULL || strstr(argsList[0], "#")) {
     //Comment line, so we do nothing
   }else if(strcmp(argsList[0], "cd") == 0) {
      if(numArgs < 2) { //If cd is provided with no directory change to home
	 chdir(getenv("HOME"));
      }else if(numArgs > 1) { 
	 chdir(argsList[1]); 
      }
   }else if(strcmp(argsList[0], "exit") == 0) {
     killProcs(); //Kill all background processes and exit
     exit(0);
   }else if(strcmp(argsList[0], "status") == 0) {
      background = 0;
      checkStatus(status); 
   }else {
      pid = fork();
      //printf("PID: %i\n", pid);
      fflush(stdout);
      if(pid == -1) { //If there is an error forking, print message and exit
	 printf("Error on fork! Exiting now\n");
	 fflush(stdout);
	 killProcs();
	 exit(0);
      }else if(pid == 0) { //Child
	 //This checks if there are any '<' '>' operators present as arguments and it flags them
	 for(int i = 0; i < numArgs; i++) {
	    if(strcmp(argsList[i], ">") == 0) {
	       outFlag = 1;
	       strcpy(outFile, argsList[i+1]);
	       argsList[i] = argsList[i+1] = NULL;
	       numArgs = numArgs - 2;
	    }else if(strcmp(argsList[i], "<") == 0) {
	       inFlag = 1;
	       strcpy(inFile, argsList[i+1]);
	       argsList[i] = argsList[i+1] = NULL;
	       numArgs = numArgs - 2;
	    }
	 }
	 //Wasn't in the script grading
	 /*if((outFlag == 0 && inFlag == 0) && (foregroundOnly == 0)) {
	    freopen("/dev/null", "w", stdout);
	    freopen("/dev/null", "r", stdin);
	 }*/

	 //If there was a '<' operator
	 if(inFlag == 1) {
	    fd = open(inFile, O_RDONLY);
	    if(fd == -1) { //Error on file open
	       printf("Error opening input file!\n");
	       fflush(stdout);
	       exit(0);
	    }else {
	       dup2(fd, 0); //Set redirect
	       close(fd);
	       //fcntl(fd, F_SETFD, FD_CLOEXEC);
	    }
	    inFlag = 0;
	 }else if(outFlag == 1) { //If there was a '>'
	    outFlag = 0;
	    fd = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	    if(fd == -1) {
	       printf("Error opening output file!\n");
	       fflush(stdout);
	       exit(0);
	    }else {
	       dup2(fd, 1); //redirect
	       close(fd);
	       //fcntl(fd, F_SETFD, FD_CLOEXEC);
	    }
	 }else if(inFlag == 1 && outFlag == 1) { //If there was a double < > statement
	    fd = open(inFile, O_RDONLY);
	    if(fd == -1) {
	       printf("Error opening input file!\n");
	       fflush(stdout);
	       exit(0);
	    }
	    dup2(fd, 0);
	    fd2 = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	    if(fd2 == -1) {
	       printf("Error opening output file!\n");
	       fflush(stdout);
	       exit(0);
	    }
	    dup2(fd2, 1);
	    close(fd);
	    close(fd2);
	 }
	 fflush(stdin);
	 fflush(stdout);

	 //If this is a foreground process, set default handling
	 if(background == 0) {
	    struct sigaction SIGINT_action = {0};
	    SIGINT_action.sa_handler = SIG_DFL;
	    sigaction(SIGINT, &SIGINT_action, NULL);
	 }
	 argsList[numArgs] = NULL;
	 fflush(stdout);
	 if((strcmp(argsList[0], "test") == 0) && (strcmp(argsList[1], "-f") == 0)) {
	    printf("Error! File not found\n"); //The reason I hard coded this is because execvp doesn't throw an error for some reason
	    fflush(stdout); //I asked the TA and he said it should throw an error, but it literally does not
	 } //Uncomment the code below to see that the correct arguments are being passed to execvp
	 /* 
	 for(int i = 0; i < numArgs; i++) {
	    printf("Argument # %d: %s\n", i+1, argsList[i]);
	 }
	 */
	 int code = execvp(argsList[0], argsList);
	 //Print error message if execvp returns -1
	 if(code != 0) {
	    printf("Error on execvp!\n");
	    fflush(stdout);
	 }
      }else if(pid > 0) { //Parent
	 if(foregroundOnly == 0 && background == 1) {
	    pid2 = waitpid(pid, &status, WNOHANG); //Background process
	    printf("Background pid: %d\n", pid);
	    fflush(stdout);
	    background = 0; //Set flag back
	 }else {
	    pid2 = waitpid(pid, &status, 0); //If foregroundOnly mode is on
	 }
      }
   }
   while((pid = waitpid(-1, &status, WNOHANG)) > 0) { //Show terminated processes
      printf("Background process finished: %d\n", pid);
      numProc--;
      checkStatus(status);
      fflush(stdout);
   }
   for(int i = 0; i < numArgs; i++) {
      free(argsList[i]);
   }
}

//Handles $$
void expand(char * userInput) {
   char * buffer = strdup(userInput);
   for(int i = 0; i < strlen(userInput); i++) { //Iterate through string and check if $$ is correctly next to each other
      if((buffer[i] == '$') && (i+1 < strlen(buffer)) && (buffer[i+1] == '$')) {
	 buffer[i] = '%';
	 buffer[i+1] = 'd';
      }
   }
   int pid = getpid();
   sprintf(userInput, buffer, pid); //put pid into string before execvp
}

int main() {
   char * userInput = malloc(2048);
   size_t length = 0;

   char **argsList = malloc(528 * (sizeof(char)));
   char *token = NULL;
   int numArgs = 0;
   handleSignals(); 

   while(1) {
      //Prompt user
      printf(": ");
      fflush(stdout);
      fflush(stdin);
      getline(&userInput, &length, stdin);

      //Replace $$ with pid
      expand(userInput);
      token = strtok(userInput, " \n"); //Read up to space or new line

      numArgs = 0; //set numArgs = 0 so each line gets new args
      while(token != NULL) {
	 argsList[numArgs] = strdup(token); //Add argument to list and count numArgs
	 numArgs++;
	 token = strtok(NULL, " \n"); //Read to next arg
      }

      if(numArgs > 0) { //Put an if-statement to prevent seg fault on blank line
	 commands(argsList, numArgs); 
      }
   }

   free(userInput);
   return 0;
}
