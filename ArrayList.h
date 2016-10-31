#define RUNNING_Z 0
#define STOPPED_Z 1
#define DONE_Z 2
#define ERROR_Z -1

typedef struct{
	pid_t child;
	char* command;
	int status;
	int pipestatus;
	bool background;
} node_data;

typedef struct{
	void* parent;
	void* left;
	void* right;
	node_data* data;
}node_t;

typedef struct{
	char* inputRedirection;
	char* outputRedirection;
	char* errorRedirection;
	char* totalcommand;
	int argc;
	char** argv;
	int pipestatus; // 1 for pipe real, 2 for pipe latter
} parsedCommand_t;

typedef struct{
	parsedCommand_t** parsedCommands;
	int commandCount;
	int arraySize;
	bool pipe;
	bool background;
	bool jobcontrol;
	char* totalpipecommand;
} commandList_t;


node_t* createNode(pid_t child_pid, char* command, int status, bool bg){

	node_t* newNode = malloc(sizeof(node_t));

	newNode->parent = NULL;
	newNode->left = NULL;
	newNode->right = NULL;
	if(child_pid > -1)
	{
	newNode->data = malloc(sizeof(node_data));
	newNode->data->child = child_pid;
	newNode->data->command = command;
	newNode->data->status = status;
	newNode->data->background = bg;
	}
	else
	{
		newNode->data = NULL;
	}
	return newNode;
}

void addToList(node_t* parent, pid_t child, parsedCommand_t* command, int status, bool bg)
{
	if(parent->data == NULL)
	{
		parent->data = malloc(sizeof(node_data));
		parent->data->child = child;
		parent->data->command = strdup(command->totalcommand);
		parent->data->status = status;
		parent->data->pipestatus = command->pipestatus;
		parent->data->background = bg;
		return;
	}
	node_t* temp = parent;
	while(temp->right != NULL)
	{
		temp = temp->right;
	}

	temp->right = createNode(child, command->totalcommand, status, bg);
	node_t* right = temp->right;
	right->left = temp;
	return;
}

bool areThereStoppedChildren(node_t* parent)
{
	node_t* temp = parent;
	do{
		if(temp->data->status == STOPPED_Z)
			return true;
		temp = temp->right;
	} while(temp !=NULL);

	return false;
}

void freeList(node_t* parent){
	node_t* temp = parent;

	while(temp->right !=NULL)
	{
		temp = temp->right;
	}

	while(temp->left != NULL)
	{
		node_t* old = temp;
		temp = temp->left;
		if(old->data->command != NULL)
			free(old->data->command);
		free(old->data);
		free(old);
	}


}

void printArrayList(node_t* parent)
{
	printf("ArrayList Printing: \n");
	bool empty = true;
	node_t* temp = parent;
	while(temp != NULL && temp->data !=NULL)
	{
		printf(" pid: %d, command: %s, status: %d\n", temp->data->child, temp->data->command, temp->data->status);
		temp = temp->right;
		empty = false;
	}
	if(empty)
		printf(" ArrayList Empty\n");
}

void printJobs(node_t* parent)
{
	bool empty = true;
	node_t* temp = parent;
	int count = 1;
	bool stopped = false;
	while(temp != NULL && temp->data !=NULL && temp->data != NULL)
	{
		if(temp->data->pipestatus == 2)
		{
			printf("SKipping this job printing\n");
			continue;
		}
		char plus_minus[2] = {0,0};
		if(temp->right == NULL)
		{
			plus_minus[0] = '+';
		}
		else{
			plus_minus[0] = '-';
		}
		if(temp->data->status == STOPPED_Z)
			stopped = true;
		printf("[%d] %s %s    %s\n",
				count,
				plus_minus,
				temp->data->status == RUNNING_Z?"Running":temp->data->status == STOPPED_Z?"Stopped":"Done",
				temp->data->command);
		temp = temp->right;
		empty = false;
		count++;
	}
	printf("Done Printing Jobs\n");
	if(stopped && temp != NULL && temp->data !=NULL && temp->data != NULL) printf("# ");
#ifdef PROJECT1_DEBUG
	if(empty)
		printf("No processes\n");
#endif
}

node_data* returnLastChild(node_t* parent)
{
	node_t* temp = parent;
	while(temp->right !=NULL)
	{
		temp = temp->right;
	}

	return temp->data;
}

node_t* removeNode(node_t* parent, pid_t child)
{
	if(parent->data == NULL)
	{
		return NULL;
	}
	if(parent->data->child == child)
	{
		free(parent->data->command);
		free(parent->data);

		if(parent->right != NULL)
		{
			node_t* right = parent->right;
			right->left = NULL;
			free(parent);
			return right;
		}
		else{
			parent->data = NULL; // signifies empty list
			return parent;
		}
	}
	bool found = false;
	node_t* temp = parent->right;
	while(1)
	{
		if(temp == NULL)
		{
			break;
		}

		if(temp->data->child == child)
		{
			if(temp->data->command!=NULL)
				free(temp->data->command);
			free(temp->data);
			node_t* leftR = temp->left;
			node_t* rightR = temp->right;
			if(leftR != NULL)
				leftR->right = rightR;
			if(rightR != NULL)
				rightR->left = leftR;
			free(temp);
			found = true;
			break;
		}
		else {
			temp = temp->right;
		}

	}
	if(found)
	return parent;
	else
		return NULL;
}

node_t* updateChildStatus(node_t* parent, int status, pid_t child)
{
	if(parent->data == NULL)
		return NULL;
	if(parent->data->child == child)
	{
		parent->data->status = status;
		return parent;
	}
	bool found = false;
	node_t* temp = parent->right;
	while(1)
	{
		if(temp == NULL)
		{
			break;
		}

		if(temp->data->child == child)
		{
			if(status > -1)
				temp->data->status = status;
			found = true;
			break;
		}
		else {
			temp = temp->right;
		}

	}
	if(found)
	return temp;
	else
		return NULL;
}
