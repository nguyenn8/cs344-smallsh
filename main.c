/*
	Name: Nancy Nguyen
	Instructor: Justin Goins
	Date: 11/3/2020
	Description: You will be able to write your own shell in C with
	smallsh which will implement a subset of shell features. You will be able to
	execute 3 commands exit, cd, and status which are built into the shell. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <ctype.h> 
//Global variables
#define LINE_LENGTH 2049
int backgroundPid[1000];
int statusVal;
int stat;
int sizePid = 0;
//0 --> in foreground
//1 --> in background
int background = 0;
//0 --> not allowed in background processes
//1 --> allow in background processes
int allowBackground = 1; 

//exit_func will exit the shell. There are no arguments.
//When this function is called the shell will kill 
//any processes that the shell has started before it
//terminates itself.
void exit_func() {
	int i; 
	int result;
	for (i = 0; i < sizePid; i++) {
		kill(backgroundPid[i], SIGKILL);
	}
	exit(0);
}

//cd functio will change the working directory of 
//smallsh. There is one argument which is the
//path of directory to change to. If there
//are not arguments specified in the shell
//it will go to the home directory.
void cd(char** arguments) {
	char* HOME = "HOME";
	if (arguments[1] == NULL) {
		chdir(getenv(HOME));
	}
	else {
		chdir(arguments[1]);
	}
}

//status_func will print out either the terminating
//signal of the last foreground process that is
//ran in the shell or the exit status. 
void status_func(){
	if (WIFSIGNALED(stat)) {
		printf("terminated by signal %d\n", WTERMSIG(stat));
	}
	else {
		if (stat == 256) {
			stat = 1;
		}
		printf("exit value %d\n", stat);
	}
}
//input function will redirect input from command line to be opened
//as read only. If the shell cannot open file for
//reading then it will print an error message
//and set exit status to be 1 which does not
//exit the shell in the program. Parameter argument
//is the input.
void input (char* argument){
	int file_descripTar;
	file_descripTar = open(argument, O_RDONLY);
	
	if (file_descripTar == -1) {
		perror(argument);
		exit(1);
	}

	int result = dup2(file_descripTar, 0);
	if (result == -1) {
		perror("dup2");
		exit(2);
	}
}

//output function will redirect via stdout and
//will be opened for write only, it will truncate if it 
//already exists or create it if it doesn't exist. If the 
//shell in the program cannot open the output file then it
//will print an error message and set exit status to be 1,
//but doesn't exit the shell of the program. The parameter
//argument is the output to be redirected.
void output(char* argument){
	int file_descripTar;
	file_descripTar = open(argument, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (file_descripTar == -1) {
		perror(argument);
		exit(1);
	}

	int result = dup2(file_descripTar, 1);
	if (result == -1) {
		perror("dup2");
		exit(2);
	}
}

//replace$$ will expand any command with any instance of 
//"$$" into the process ID. It will loop through the 
//arguments in the command line to find if it contains
//"$$" then null the arguments with "$$" so that when 
//it is formatted $$ will not be displayed with the output 
//of the process ID.
char** replace$$(int argc, char** argument, char IDlist[]) {
	int i;
	int j; 
	int len;
	for (i = 0; i < argc; i++) {
		len = strlen(argument[i]);
		for (j = 0; j < len; j++)
		{
			if (argument[i][j] == '$' && argument[i][j + 1] == '$') 
			{
				argument[i][j] = NULL;
				argument[i][j + 1] = NULL;
				sprintf(IDlist, "%s%d", argument[i], getpid());
				argument[i] = IDlist;
			}
		}
		/*printf("arg[%d] = |%s|\n", i, argument[i]);*/
	}
	return argument;
}

//Signal handler for SIGINT
void SIGINT_func(int signo) {
	char* message = "Caught SIGINT\n";
	write(STDOUT_FILENO, message, 39);
	fflush(stdout);
}


//Sigtstp_func function will display messages if the 
//user does CTRL-Z command it will display a message if
//it is running on the foreground or not. Then the shell
//returns back to the normal condition where & is honored 
//for subsequent commands to be executed in the background. 
void Sigtstp_func(int signo) {
	char* message1 = "Entering foreground-only mode (& is now ignored)\n";
	char* message2 = "Exiting foreground-only mode\n";
	char* message3 = ":";
	allowBackground = !allowBackground;
	if (allowBackground == 1) {
		write(STDOUT_FILENO, message2, 30);
		write(1, message3, 2);
		fflush(stdout);
	}
	else {
		write(STDOUT_FILENO, message1, 50);
		write(1, message3, 2);
		fflush(stdout);
	}
}

//checkBackgroundPid function will check if the child is terminated.
//WNOHANG is specified. Will use macros to check child process status
//which is returned by waitpid to see if exited normally or which 
//signal was raised by child process. Then each will display
//a message associated to those conditions. 
void checkBackgroundPid(){
	int i;
	int termRet;
	int stat2;
	for (i = 0; i < sizePid; i++) {
		termRet = waitpid(backgroundPid[i], &stat2, WNOHANG);
		if (termRet == backgroundPid[i]) {
			if (WIFEXITED(stat2)) {
				printf("background pid %d is done: exit value %d \n", backgroundPid[i], WEXITSTATUS(stat2));
				fflush(stdout);
			}
			else if (WIFSIGNALED(stat2)) {
				printf("background pid %d is done: terminated by signal %d \n", backgroundPid[i], WTERMSIG(stat2));
				fflush(stdout);
			}
		}
	}
}

int main() {
	char IDlist[10];
	char* cmdPrmpt;
	char buff[LINE_LENGTH];
	int size;
	char* token;
	char* saveptr;
	char** arguments = malloc(sizeof(char*) * 513);
	pid_t childPid;
	pid_t spawnpid;

	int inputFile;
	int outputFile;

	//Initialized structs to be empty for
	//SIGINT_action and SIGTSTP_action
	struct sigaction SIGINT_action = { 0 };
	struct sigaction SIGTSTP_action = { 0 };

	//sigint
	//Fill out the SIGINT_action struct
	//Register signal ignore as the signal
	//handler
	SIGINT_action.sa_handler = SIG_IGN;
	//Block all the catchable signals while
	//SIG_IGN is running
	sigfillset(&SIGINT_action.sa_mask);
	//No flag set
	SIGINT_action.sa_flags = 0;
	//Install signal handler
	sigaction(SIGINT, &SIGINT_action, NULL);

	//sigtstp
	//Fill out the SIGTSTP_action struct
	//Register Sigtstp_func as the signal
	//handler
	SIGTSTP_action.sa_handler = Sigtstp_func;
	//Block all the catchable signals while
	//Sigtstp_func is running
	sigfillset(&SIGTSTP_action.sa_mask);
	//Flag is set to restart
	SIGTSTP_action.sa_flags = SA_RESTART;
	//Install signal handler
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	while (1)
	{
		//Check if background pid to see if terminated
		checkBackgroundPid();
		//Prompt user with colon
		memset(buff, 0, LINE_LENGTH);
		printf(":");
		fflush(stdout);
		fgets(buff, LINE_LENGTH, stdin);
		//Allow blank line and comments when running program
		//Check if argument is newline, #, or space
		//and will reprompt after checking for these 
		//arguments.
		int argc = 0;
		if (buff[0] == 10 || buff[0] == '#' || buff[0] == ' ') {
			continue;
		}
		//Parse string by space and newline, save to array of strings
		token = strtok_r(buff, " \n", &saveptr);
		while (token != NULL)
		{
			arguments[argc] = token;
			argc++;
			token = strtok_r(NULL, " \n", &saveptr);
		}

		arguments[argc] = NULL;
		//Expand any $$ in command line
		arguments = replace$$(argc, arguments, IDlist);
		//Check if the last argument is & then it means
		//that it will run in the background in program
		if (strcmp(arguments[argc - 1], "&") == 0)
		{
			background = 1;
			arguments[argc - 1] = NULL;
		}
		else
		{
			background = 0;
		}
		//Check the built-in commands then shell will
		//handle is by itself while the rest will
		//be passed to the member of the execvp function
		if (strcmp(arguments[0], "exit") == 0) {
			exit_func();
		}
		else if (strcmp(arguments[0], "cd") == 0) {
			cd(arguments);
		}
		else if (strcmp(arguments[0], "status") == 0) {
			status_func();
		}
		else
		{
			//Other commands for non-built commands
			//will fork off a child.
			//If fork is successful, the value of 
			//spawnpid will be 0 in the chiild, the
			//child's pid in the parent
			spawnpid = fork();
			switch (spawnpid)
			{
			case -1:
				//In branch this code will be exected by the
				//parent when fork() fails and the creation of
				//child process fails as well
				perror("smallsh");
				exit(1);
				break;

			case 0:
				//Check if there are any special symbols
				//to be redirected followed by filename word
				//that appears after the arguments.
				//If the user doesn't redirect the standard
				//output and input for the background command
				//then the output and input will be redirected
				//to "/dev/null"
				for (int i = 0; i < argc; i++)
				{
					if (arguments[i] != NULL)
					{
						if (strcmp(arguments[i], "<") == 0)
						{
							input(arguments[i + 1]);
						}
						else if (strcmp(arguments[i], ">") == 0)
						{
							output(arguments[i + 1]);
						}
						else if (!strcmp(arguments[i], "<"))
						{
							inputFile = open("/dev/null", O_RDONLY, 0644);
							if (inputFile == -1) {
								perror("open");
								exit(1);
							}

							int resultInput = dup2(inputFile, 0);
							if (resultInput == -1) {
								perror("dup2");
								exit(2);
							}
						}
						else if (!strcmp(arguments[i], ">")) {
							outputFile = open("/dev/null", O_WRONLY | O_TRUNC | O_CREAT, 0644);
							if (outputFile == -1) {
								perror("open");
								exit(1);
							}

							int resultOutput = dup2(outputFile, 1);
							if (resultOutput == -1) {
								perror("dup2");
								exit(2);
							}
						}
					}
				}
				//Null arugments after redirecting input and output 
				//redirection
				int i = 0;
				while (arguments[i] != NULL)
				{
					if (strcmp(arguments[i], "<") == 0 || strcmp(arguments[i], ">") == 0)
					{
						arguments[i] = NULL;
						break;
					}
					i++;
				}

				//When not in the background child will
				//ignore 
				if (background == 0) {
					struct sigaction default_action = { 0 };
					default_action.sa_handler = SIG_DFL;
					sigaction(SIGINT, &default_action, NULL);
				}
				//children will ignore SIGTSTP
				struct sigaction ignoreSIGTSTP = { 0 };
				ignoreSIGTSTP.sa_handler = SIG_IGN;
				sigaction(SIGTSTP, &ignoreSIGTSTP, NULL);

				//Child will use function from exec() family of 
				//functions to run command and display error message
				//if path filename in argument is found or not
				if (execvp(arguments[0], arguments))
				{
					stat = 1;
					perror(arguments[0]);
					exit(stat);
				}
				break;

			default:
				//Check will parent process in background 
				//then display the background process ID
				if (background == 1 && allowBackground == 1)
				{
					backgroundPid[sizePid] = spawnpid;
					printf("background pid is %d\n", spawnpid);
					fflush(stdout);
					sizePid++;
				}
				else
				{
					//Parent process creates child process then 
					//calls waitpid to check status of child proccess has 
					//terminated or not
					childPid = waitpid(spawnpid, &stat, 0);
					//Check if macro is true of signal was raised.
					//Display message to notify if parent was terminated
					//by signal
					if (WIFSIGNALED(stat)) {
						printf("terminated by signal %d\n", WTERMSIG(stat));
						fflush(stdout);
					}
				}
				break;
			}
		}
	}
	return 0;
}