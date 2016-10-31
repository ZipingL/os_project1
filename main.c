#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "ArrayList.h"
//#define PROJECT1_DEBUG 0

		int pid[2] = {-1,-1};

node_t* arrayListOfProcesses = NULL;
commandList_t commandList;

void my_handler(sig_t s){

    switch((int)s)
    {
		case SIGINT:
		{
#ifdef PROJECT1_DEBUG
			printf("Parent Received Control C, ignoring");
#endif
			if(pid[0] != -1 && kill(pid[0],0) == 0)
			{
				//printf("Killed first child %d", pid[0]);
				kill(-pid[0], SIGINT);
			}
			/*
			if(pid[1] != -1 && kill(pid[1], 0) == 0)
			{
				//printf("Killed second child %d", pid[1]);
				kill(pid[1], SIGINT);
			} */
			break;
		}
		case SIGTSTP:
		{
#ifdef PROJECT1_DEBUG
			printf("Caught Control Z in SIGSTTP HANDLER child %d parent %d\n", pid[0], getpid());
#endif
			int kill_id = kill(-pid[0], SIGTSTP);
#ifdef PROJECT1_DEBUG
			printf("Kill sigtstp id %d\n", kill_id);
#endif
			break;
		}
		case SIGCHLD:
		{
#ifdef PROJECT1_DEBUG
			printf("Caught signal chld %d\n",s);
#endif
		    pid_t pid;
		    int   status;
		    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
		    {
#ifdef PROJECT1_DEBUG
		    	printf("SIGCHLD found: %d, mypid %d: \n", pid, getpid());
		    	printJobs(arrayListOfProcesses);
#endif

				node_t* child_bg = updateChildStatus(arrayListOfProcesses, -1, pid);
				if(child_bg != NULL && child_bg->data->background == true)
				{
#ifdef PROJECT1_DEBUG
					printJobs(arrayListOfProcesses);
#endif
					updateChildStatus(arrayListOfProcesses, DONE_Z, pid);

				}
				node_t* temp = removeNode(arrayListOfProcesses, pid);

				if(temp == NULL)
				{
#ifdef PROJECT1_DEBUG
					printf("Fatal error: child not found for removal\n");
#endif
				}
				else
				{
					arrayListOfProcesses = temp;
				}
		    }
		    if(pid == -1){
#ifdef PROJECT1_DEBUG
		    printf("When catching sig child, maybeError happened: %s\n", strerror(errno));
#endif
		    }
		}
    }

}
void my_handler2(sig_t s){


    switch((int)s)
    {
		case SIGTSTP:
		{
#ifdef PROJECT1_DEBUG
			printf("Caught Control Z %d, my pid %d\n", pid[0], getpid());
#endif
			if(pid[0] != -1 && kill(pid[0],0) == 0)
			{
				int kill_r = kill(-pid[0], SIGTSTP);
				updateChildStatus(arrayListOfProcesses, STOPPED_Z, pid[0]);
				printJobs(arrayListOfProcesses);
#ifdef PROJECT1_DEBUG
				printf("in my_handler2 Stopped first child, z, %d\n", kill_r);
#endif

			    if(pid == -1) {
#ifdef PROJECT1_DEBUG
			    printf("When sigtstp sig child, maybeError happened: %s\n", strerror(errno));
#endif
			    }

			}
			/*if(pid[0] != -1 && kill(pid[1], 0) == 0)
			{
				printf("Killed second child, z");
				kill(pid[1], SIGTSTP);
			}*/
			break;
		}
    }

}

void printCommandList(commandList_t* commandList, int index)
{
	printf("inputRedirection: %s \noutputred: %s \nerrorred: %s \nargc: %d \ncommand: %s\n",
			commandList->parsedCommands[index]->inputRedirection,
			commandList->parsedCommands[index]->outputRedirection,
			commandList->parsedCommands[index]->errorRedirection,
			commandList->parsedCommands[index]->argc,
			commandList->parsedCommands[index]->argv[0]);

	if(commandList->parsedCommands[index]->argv[commandList->parsedCommands[index]->argc] == NULL)
		printf("printCommandList detected argv setup correctly, yes null\n");

}


bool parseSingleCommand(char* input, commandList_t* commandList, int pipestatus)
{
#ifdef PROJECT1_DEBUG
	printf("parseSingl Input %s\n", input);
#endif
	// Initialize the structure for storing command data, meta data
	parsedCommand_t* parsedCommand = malloc(sizeof(*parsedCommand));
	parsedCommand->argv = NULL;
	parsedCommand->inputRedirection = NULL;
	parsedCommand->errorRedirection = NULL;
	parsedCommand->outputRedirection = NULL;
	parsedCommand->argc = 0;
	parsedCommand->pipestatus = pipestatus;
	parsedCommand->totalcommand = strdup(pipestatus == 1? commandList->totalpipecommand: input);

	char* inputWords[10];
	int i = 0;
	inputWords[i] = strtok(input, " ");
	while(inputWords[i] != NULL)
	{
		i++;
		inputWords[i] = strtok(NULL, " ");
	}


	for(int j = 0; j < i; j++)
	{

		bool commandArgs = true;
		// Actual Command
		if(j == 0)
		{
			if(parsedCommand->argv == NULL)
				parsedCommand->argv = malloc(sizeof(char*)*(i+1)); // set argv size to total number of tokenized strings + 1 for null, to insure it's biggest size
			parsedCommand->argv[parsedCommand->argc] = malloc(sizeof(char)*(1+strlen(inputWords[j])) );
			strcpy(parsedCommand->argv[parsedCommand->argc], inputWords[j]);
			parsedCommand->argc++;
			parsedCommand->argv[parsedCommand->argc] = NULL;
			commandArgs = false;
			if(strcmp(inputWords[j], "bg") == 0 |
			   strcmp(inputWords[j], "fg") == 0 |
			   strcmp(inputWords[j], "jobs") == 0)
			{
				commandList->jobcontrol = true;
			}
			else{
				commandList->jobcontrol = false;
			}
		}

		else if( strcmp(inputWords[j], ">") == 0 )
		{
			parsedCommand->outputRedirection = malloc(sizeof(char)*(1+strlen(inputWords[j+1])));
			strcpy(parsedCommand->outputRedirection, inputWords[j+1]);
			j++;
			commandArgs = false;
		}

		else if( strcmp(inputWords[j], "<") == 0 )
		{
			parsedCommand->inputRedirection = malloc(sizeof(char)*(1+strlen(inputWords[j+1])));
			strcpy(parsedCommand->inputRedirection, inputWords[j+1]);
			j++;
			commandArgs = false;
		}

		else if( strcmp(inputWords[j], "2>") == 0 )
		{
			parsedCommand->errorRedirection = malloc(sizeof(char)*(1+strlen(inputWords[j+1])));
			strcpy(parsedCommand->errorRedirection, inputWords[j+1]);
			j++;
			commandArgs = false;
		}

		else if(commandArgs && *(inputWords[j]) != '&')
		{
			parsedCommand->argv[parsedCommand->argc] = malloc(sizeof(char)*(1+strlen(inputWords[j])));
			strcpy(parsedCommand->argv[parsedCommand->argc], inputWords[j]);
			parsedCommand->argc++;
		}
	}

	parsedCommand->argv[parsedCommand->argc] = NULL;

	commandList->parsedCommands[commandList->commandCount++] = parsedCommand;
#ifdef PROJECT1_DEBUG
	printf("end ParseSignleCommand: %s\n", parsedCommand->totalcommand);
#endif
	return true;
}

bool parseInput(char* input, commandList_t* commandList)
{
	char* input_temp = input;
	int i = 0;
	bool pipe = false;
	commandList->pipe = false;
	commandList->background = false;
	commandList->totalpipecommand = NULL;
	int pipe_i = 0;
	while(input_temp[i] != NULL)
	{

		if(input_temp[i] == '|')
		{
			commandList->pipe = true;
			commandList->totalpipecommand = strdup(input);
			pipe = true;
			pipe_i = i;
		}
		else if(input_temp[i] == '&')
		{
			commandList->background = true;
		}
		i++;
	}
	if(pipe_i > 0)
		i = pipe_i;
	// Create a copy of input
	char* input_copy = malloc(sizeof(char)*(i+1));
	for(int j = 0; j < (i); j++)
	{
		input_copy[j] = input_temp[j];
	}
	input_copy[pipe_i > 0 ? i-1 : i] = NULL;

	parseSingleCommand(input_copy, commandList, commandList->pipe == true ? 1 : 0);
	free(input_copy);

	// Do it again, for any more commands
	if(pipe)
	{
		int j = i + 2; // offset is two since we want to skip the | and the space after
#ifdef PROJECT1_DEBUG
		printf("%s\n", input_temp + i + 2);
		printf("original %s\n", input_temp);
#endif
		int command_length = strlen(input_temp + i + 2);
		char* input_copy = malloc(sizeof(char)*(command_length + 1));

		int index = 0;
		while(input_temp[j] != NULL)
		{
			input_copy[index++] = input_temp[j];
			j++;
		}
#ifdef PROJECT1_DEBUG
		printf("New copy of piped: %s\n, length: %d\n", input_copy, command_length);
#endif
		input_copy[index] = NULL;
		parseSingleCommand(input_copy, commandList, 2);
		free(input_copy);
	}



	return true;
}

bool redirectIO(commandList_t* commandList, int index, int* pipefd)
{
#ifdef PROJECT1_DEBUG
	printf("boolean %d\n", commandList->pipe);
#endif
	if(commandList->pipe)
	{
	    if (index == 1) {    /* Child reads from pipe */
		close(pipefd[1]);          /* Closes unused write end */
		dup2(pipefd[0], STDIN_FILENO);
		close(pipefd[0]);

	    } else {            /* Child of Child writes argv[1] to pipe */
		close(pipefd[0]);          /* Close unused read end */
	    dup2(pipefd[1], STDOUT_FILENO);
	    close(pipefd[1]);
	    }
	} else
	{
		parsedCommand_t* command = commandList->parsedCommands[index];
		if(command->inputRedirection != NULL)
		{
			if(access(command->inputRedirection, 0) != -1)
			{
				FILE* fileN = fopen(command->inputRedirection, "r");
				int open = fileno(fileN);
				dup2(open, STDIN_FILENO);
				close(open);
			} else
			{
				return false;
			}
		}
		if(command->outputRedirection != NULL)
		{
			FILE* fileN = fopen(command->outputRedirection, "a+");
			int open = fileno(fileN);
			dup2(open, STDOUT_FILENO);;
			close(open);
		}
		if(command->errorRedirection != NULL)
		{
			FILE* fileN = fopen(command->errorRedirection, "a+");
			int open = fileno(fileN);
			dup2(open, STDERR_FILENO);
			close(open);
		}
	}

	return true;
}

void runJobControl(commandList_t* commandList)
{
	//printf("3\n");
	node_data* lastchild = returnLastChild(arrayListOfProcesses);
	char* job_command = commandList->parsedCommands[0]->argv[0];
	if(strcmp(job_command, "bg") == 0 )
	{
		if(lastchild == NULL)
		{
			return;
		}
		lastchild->status = RUNNING_Z;
		lastchild->background = true;
		commandList->background = true;
		pid[0] = lastchild->child;
		printJobs(arrayListOfProcesses);
		int kill_id = kill(lastchild->child, SIGCONT);
#ifdef PROJECT1_DEBUG
	    if(kill_id == -1)
	    printf("calling bg, error happened:  %s\n", strerror(errno));
#endif
	}
	else if(strcmp(job_command, "fg")==0)
	{
#ifdef PROJECT1_DEBUG
		if(lastchild != NULL)
		printf("FG pid %d\n", lastchild->child);
#endif
		if(lastchild == NULL)
		{
			return;
		}
		lastchild->status = RUNNING_Z;
		lastchild->background = false;
		pid[0] = lastchild->child;
		printf("%s\n", lastchild->command);
		int kill_id = kill(lastchild->child, SIGCONT);
#ifdef PROJECT1_DEBUG
	    if(kill_id == -1)
	    printf("when fg, error happened:  %s\n", strerror(errno));
#endif
	}
	else if(strcmp(job_command, "jobs") == 0)
	{
		printJobs(arrayListOfProcesses);
	}
}

bool runCommand(commandList_t* commandList, int* pid, int index, int* pipefd, int* status)
{

	if(commandList->jobcontrol == true)
	{
        if(commandList->jobcontrol == true)
        {
        	runJobControl(commandList);
        }
        return true;
	}

    signal(SIGINT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    pid[index] = fork();
    if (pid[index] < 0) {
        // fork failed; exit
        fprintf(stderr, "fork failed\n");
        exit(1);
    } else if (pid[index] == 0) {


        // CHILDREN

    	if(commandList->pipe == true)
    	{
    		if(index == 0)
    		{
    			setsid();
    		}
    		else
    		{
    			setpgid(getpid(), arrayListOfProcesses->data->child);
    		}
    	} else{
    		//setpgid(getpid(), getpid());
    	}
/*
    	if(commandList->pipe == true && index == 0)
    	{
#ifdef PROJECT1_DEBUG
    		printf("WARNNING REPEATINGDDDDDDDDDDDDDDDDDDDD\n");
#endif
    		runCommand(commandList, pid, index+1, pipefd, status);
    	} */

#ifdef PROJECT1_DEBUG
        printf("hello, I am child (pid:%d)\n", (int) getpid());
        printCommandList(commandList,index);
#endif
        redirectIO(commandList, index, pipefd);


        	execvp(commandList->parsedCommands[index]->argv[0], commandList->parsedCommands[index]->argv);
        	printf("Execvp for %d%s command \"%s\" failed\n",index+1, (index+1)==1 ?"st":(index+1)==2?"nd":"th",
        		commandList->parsedCommands[index]->argv[0]);
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

    } else {

        signal(SIGINT, &my_handler);
        signal(SIGTSTP, &my_handler);
        signal(SIGCHLD, &my_handler);

    	/*if(!commandList->background)
    	if(waitpid(-1,&(status[index]),WUNTRACED | WCONTINUED))
    		{
    			printf("hello after waiting for child %d\n", pid[index]);
    		} */
        // parent goes down this path (original process)

    	//Parent sends Child to background
    	if(commandList->background)
    	{
    		//printf("fork kill sgitstp\n");
    		//ptrace(PTRACE_ATTACH, pid[index], 0L, 0L);

    		//kill(pid[index], SIGTSTP);
    	}
#ifdef PROJECT1_DEBUG
        printf("hello, I am parent of %d (pid:%d)\n",
	       pid[index], (int) getpid());

        printf("add to list %s\n", commandList->parsedCommands[index]->totalcommand);
#endif

        if(index == 0)
        addToList(arrayListOfProcesses, pid[index], commandList->parsedCommands[index],
        		RUNNING_Z, commandList->background);

    }

    return true;

}

void freeCommandList(commandList_t* commandList)
{
	int i = 0;
	for(i = 0; i < commandList->commandCount; i++);
	{
		if(commandList->parsedCommands[i]->errorRedirection != NULL)
		{
			free(commandList->parsedCommands[i]->errorRedirection);
		}
		if(commandList->parsedCommands[i]->outputRedirection != NULL)
		{
			free(commandList->parsedCommands[i]->outputRedirection);
		}
		if(commandList->parsedCommands[i]->inputRedirection != NULL)
		{
			free(commandList->parsedCommands[i]->inputRedirection);
		}

		if(commandList->parsedCommands[i]->argv != NULL)
		{
			for(int j = 0; j < commandList->parsedCommands[i]->argc; j++)
			{
				free(commandList->parsedCommands[i]->argv[0]);
			}
		}

	}

	free(commandList->parsedCommands);
}




int main(int argc, char **argv)
{


	bool exitStatus = false;
	char input[2001];
	int status[2] = {-1,-1};


	arrayListOfProcesses = createNode(-1, NULL, 0, 0);
	// Shell takes orders now
	while(!exitStatus)
	{

		// Initialize a list for storing commands

		commandList.arraySize = 2;
		commandList.parsedCommands = malloc(sizeof(*commandList.parsedCommands)*commandList.arraySize);
		commandList.commandCount = 0;
		commandList.pipe = false;

		printf("# ");
		char* fgetN = fgets(input, 2001, stdin);
		if(fgetN == NULL)
			return 0;

		if(fgetN[0] == '\n')
		{
			continue;
		}

		size_t ln = strlen(input) - 1;
		if (*input && input[ln] == '\n')
		    input[ln] = '\0';

		parseInput(input, &commandList);


		int pipefd[2] = {-1, -1};
	#ifdef PROJECT1_DEBUG
		printf("parent bool check %d\n", commandList.pipe);
	#endif
		if(commandList.pipe)
		{
			if (pipe(pipefd) == -1) {
			perror("pipe");
			exit(EXIT_FAILURE);
			}
		}
		for(int i = 0; i < commandList.commandCount; i++)
		{
			if(i ==2)
			{
				commandList.parsedCommands[i]->pipestatus = 2;
			}
			runCommand(&commandList, pid, i, pipefd, &status);

		}
		//printf("1\n");

		if(commandList.background == false)
		{
			//printf("2\n");
			pid_t wait_id = -1;
			int index = 0;
			do
			{
#ifdef PROJECT1_DEBUG
				printArrayList(arrayListOfProcesses);
				printf("background: %d\n", commandList.background);
#endif
				node_data* data_child= returnLastChild(arrayListOfProcesses);
				//node_data* data_child = NULL;
#ifdef PROJECT1_DEBUG
				if(data_child != NULL)
				printf("data_child pid %d", data_child->child);
#endif
				if (strcmp(commandList.parsedCommands[0]->totalcommand, "jobs") != 0 && commandList.background == false && data_child != NULL && data_child->status!=STOPPED_Z){
				wait_id = waitpid(data_child->child,&(status[index]),WUNTRACED);
				}
				else
				wait_id = 0;
#ifdef PROJECT1_DEBUG
				printf("Returned from wait, wait_id %d\n", wait_id);
#endif
				int ziping_wait_status = -1;
				bool wait_status;
				///Check for exit status only if we actually called waitpid()
				if(wait_id != 0)
				{
					wait_status = WIFEXITED(status[index]);
				}
				if (wait_status) {

					ziping_wait_status = 0; // exited
#ifdef PROJECT1_DEBUG
					printf("child %d exited, status=%d\n", pid, WEXITSTATUS(status[index]));
#endif
				} else {
					wait_status = WIFSTOPPED(status[index]);
					if(wait_status)
					{
#ifdef PROJECT1_DEBUG
						printf("parent waitpid has caught stopped child %d\n", wait_id);
#endif
						ziping_wait_status = 1; // stopped
					}
				}

				if(ziping_wait_status == 1)
				{
					updateChildStatus(arrayListOfProcesses, STOPPED_Z, wait_id);
					//printf("Look at this: \n");
					//printJobs(arrayListOfProcesses);
					break;
				}



				else if(ziping_wait_status == 0)
				{
#ifdef PROJECT1_DEBUG
						printf("hello after waiting for child %d\n", pid[index]);
#endif
						node_t* temp = NULL;
						//updateChildStatus(arrayListOfProcesses, -1, pid[index])->data

						node_data* lastChild = returnLastChild(arrayListOfProcesses);
						// Remove only if child has exited
						if( lastChild !=NULL && lastChild->status!=STOPPED_Z)
							temp = removeNode(arrayListOfProcesses, wait_id);
#ifdef PROJECT1_DEBUG
						printf("return from removeNodeMain\n");
#endif
						if(temp == NULL)
						{
#ifdef PROJECT1_DEBUG
							printf("Fatal error: child not found for removal, or removal not done\n");
#endif
						}
						else
						{
							arrayListOfProcesses = temp;
						}
				}

			} while(wait_id > 0);
#ifdef PROJECT1_DEBUG
			printf("Exit Do\n");
#endif
		}
	}

    return 0;
}


