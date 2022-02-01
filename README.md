# cs344-smallsh
Files included in zip:
---main.c
---Makefile
---README.txt
---p3testscript

To compile program using the following commands steps:
Step 1: gcc --std=gnu99 -o smallsh main.c 
Step 2: ./smallsh

Note: To run grading script in the file if not executing each command in step 2, 
but just running the grading script, p3testscript, in the directory then can be 
run as, p3testscript. Can replace command ./smallsh to p3testscript in step 2.

Assumption:
---A command is made up of words separated by spaces. 

---Will sometimes require user to enter twice after inputting in command line to
---kill a process ID to display message that ID was terminated by signal 15.
Example:
:sleep 10 &
background pid is 178502
:kill -15 178502
:(user will press enter)
:background pid 178502 is done: terminated by signal 15

Write smallsh own shell in C. 

smallsh will implement a subset of features of well-known shells, such as bash. 

Program will:

- Provide a prompt for running commands
- Handle blank lines and comments, which are lines beginning with the # character
- Provide expansion for the variable $$
- Execute 3 commands exit, cd, and status via code built into the shell
- Execute other commands by creating new processes using a function from the exec family of functions
- Support input and output redirection
- Support running commands in foreground and background processes
- Implement custom handlers for 2 signals, SIGINT and SIGTSTP
