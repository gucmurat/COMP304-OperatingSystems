#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h> 
#include <ctype.h>

#define MAX_BUF 4096

const char * sysname = "shellax";

enum return_codes {
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};

struct command_t {
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3]; // in/out redirection
	struct command_t *next; // for piping
};

void chatroom_func(char *room_name, char *user_name);
int uniq_func(int flag);
void vigenere_func(char *mode, char *plaintext, char *key);

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t * command)
{
	int i=0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background?"yes":"no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete?"yes":"no");
	printf("\tRedirects:\n");
	for (i=0;i<3;i++)
		printf("\t\t%d: %s\n", i, command->redirects[i]?command->redirects[i]:"N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i=0;i<command->arg_count;++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}


}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i=0; i<command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i=0;i<3;++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next=NULL;
	}
	free(command->name);
	free(command);
	return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
    gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters=" \t"; // split at whitespace
	int index, len;
	len=strlen(buf);
	while (len>0 && strchr(splitters, buf[0])!=NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len>0 && strchr(splitters, buf[len-1])!=NULL)
		buf[--len]=0; // trim right whitespace

	if (len>0 && buf[len-1]=='?') // auto-complete
		command->auto_complete=true;
	if (len>0 && buf[len-1]=='&') // background
		command->background=true;

	char *pch = strtok(buf, splitters);
	command->name=(char *)malloc(strlen(pch)+1);
	if (pch==NULL)
		command->name[0]=0;
	else
		strcpy(command->name, pch);

	command->args=(char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index=0;
	char temp_buf[1024], *arg;
	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch) break;
		arg=temp_buf;
		strcpy(arg, pch);
		len=strlen(arg);

		if (len==0) continue; // empty arg, go for next
		while (len>0 && strchr(splitters, arg[0])!=NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len>0 && strchr(splitters, arg[len-1])!=NULL) arg[--len]=0; // trim right whitespace
		if (len==0) continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|")==0)
		{
			struct command_t *c=malloc(sizeof(struct command_t));
			int l=strlen(pch);
			pch[l]=splitters[0]; // restore strtok termination
			index=1;
			while (pch[index]==' ' || pch[index]=='\t') index++; // skip whitespaces

			parse_command(pch+index, c);
			pch[l]=0; // put back strtok termination
			command->next=c;
			continue;
		}

		// background process
		if (strcmp(arg, "&")==0)
			continue; // handled before

		// handle input redirection
		redirect_index=-1;
		if (arg[0]=='<')
			redirect_index=0;
		if (arg[0]=='>')
		{
			if (len>1 && arg[1]=='>')
			{
				redirect_index=2;
				arg++;
				len--;
			}
			else redirect_index=1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index]=malloc(len);
			strcpy(command->redirects[redirect_index], arg+1);
			continue;
		}

		// normal arguments
		if (len>2 && ((arg[0]=='"' && arg[len-1]=='"')
			|| (arg[0]=='\'' && arg[len-1]=='\''))) // quote wrapped arg
		{
			arg[--len]=0;
			arg++;
		}
		command->args=(char **)realloc(command->args, sizeof(char *)*(arg_index+1));
		command->args[arg_index]=(char *)malloc(len+1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count=arg_index;
	return 0;
}

void prompt_backspace()
{
	putchar(8); // go back 1
	putchar(' '); // write empty over
	putchar(8); // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index=0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

    // tcgetattr gets the parameters of the current terminal
    // STDIN_FILENO will tell tcgetattr that it should write the settings
    // of stdin to oldt
    static struct termios backup_termios, new_termios;
    tcgetattr(STDIN_FILENO, &backup_termios);
    new_termios = backup_termios;
    // ICANON normally takes care that one line at a time will be processed
    // that means it will return if it sees a "\n" or an EOF or an EOL
    new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
    // Those new settings will be set to STDIN
    // TCSANOW tells tcsetattr to change attributes immediately.
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);


    //FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state=0;
	buf[0]=0;
  	while (1)
  	{
		c=getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c==9) // handle tab
		{
			buf[index++]='?'; // autocomplete
			break;
		}

		if (c==127) // handle backspace
		{
			if (index>0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c==27 && multicode_state==0) // handle multi-code keys
		{
			multicode_state=1;
			continue;
		}
		if (c==91 && multicode_state==1)
		{
			multicode_state=2;
			continue;
		}
		if (c==65 && multicode_state==2) // up arrow
		{
			int i;
			while (index>0)
			{
				prompt_backspace();
				index--;
			}
			for (i=0;oldbuf[i];++i)
			{
				putchar(oldbuf[i]);
				buf[i]=oldbuf[i];
			}
			index=i;
			continue;
		}
		else
			multicode_state=0;

		putchar(c); // echo the character
		buf[index++]=c;
		if (index>=sizeof(buf)-1) break;
		if (c=='\n') // enter key
			break;
		if (c==4) // Ctrl+D
			return EXIT;
  	}
  	if (index>0 && buf[index-1]=='\n') // trim newline from the end
  		index--;
  	buf[index++]=0; // null terminate string

  	strcpy(oldbuf, buf);

  	parse_command(buf, command);

  	// print_command(command); // DEBUG: uncomment for debugging

    // restore the old settings
    tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  	return SUCCESS;
}
int process_command(struct command_t *command);
int process_command(struct command_t *command);
int uniq_func(int flag);
int wiseman_func(int numb);
int pipe_execute(struct command_t *command);
long long timeInMilliseconds(void);
int main()
{
	while (1)
	{
		struct command_t *command=malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code==EXIT) break;

		code = process_command(command);
		if (code==EXIT) break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command)
{
	int r;
	if (strcmp(command->name, "")==0) return SUCCESS;

	if (strcmp(command->name, "exit")==0)
		return EXIT;

	if (strcmp(command->name, "cd")==0)
	{
		if (command->arg_count > 0)
		{
			r=chdir(command->args[0]);
			if (r==-1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			return SUCCESS;
		}
	}
	
	if (strcmp(command->name, "chatroom")==0)
	{
		if(command->arg_count==2){
			chatroom_func(command->args[0],command->args[1]);
			return SUCCESS;
		}
		else {
			printf("%s\n","Wrong number of arguments!");
			return EXIT;
		}
	}

	if(command -> next != NULL){
        	return pipe_execute(command);
	}

	if (strcmp(command->name, "myuniq")==0){
		if(command -> args[0] == NULL){
			uniq_func(0);
		}else if((strcmp(command->args[0],"-c")== 0) || (strcmp(command -> args[0],"--count") == 0)){
			uniq_func(1);
		}else{
		
		}
		return SUCCESS;
	}
	if (strcmp(command->name, "wiseman")==0){
			if(command -> arg_count != 1){
				printf("Wrong number of arguments");
				return EXIT;
			}
			wiseman_function(atoi(command -> args[0]));
			return SUCCESS;
	}

	if (strcmp(command->name, "vigenere")==0)
	{
		if(command->arg_count==3){
			vigenere_func(command->args[0],command->args[1],command->args[2]);
			return SUCCESS;
		}
		else {
			printf("%s\n","Wrong number of arguments!");
			return EXIT;
		}
	}

	if (strcmp(command->name, "reflex")==0){
			reflex_func();
			return SUCCESS;
	}

	pid_t pid=fork();
	if (pid==0) // child
	{
		/// This shows how to do exec with environ (but is not available on MacOs)
	    // extern char** environ; // environment variables
		// execvpe(command->name, command->args, environ); // exec+args+path+environ

		/// This shows how to do exec with auto-path resolve
		// add a NULL argument to the end of args, and the name to the beginning
		// as required by exec

		// increase args size by 2
		command->args=(char **)realloc(
			command->args, sizeof(char *)*(command->arg_count+=2));

		// shift everything forward by 1
		for (int i=command->arg_count-2;i>0;--i)
			command->args[i]=command->args[i-1];

		// set args[0] as a copy of name
		command->args[0]=strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count-1]=NULL;

		
		//Handle the redirection issue
		int fw;
		if(command -> redirects[0] != NULL){
			fw=open(command -> redirects[0], O_RDONLY, S_IRUSR | S_IWUSR);
			dup2(fw,0);
		}

		if(command -> redirects[1] != NULL){
			fw=open(command -> redirects[1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR | S_IWUSR);
			dup2(fw,1);
		}

		if(command -> redirects[2] != NULL){
			fw=open(command -> redirects[2], O_APPEND|O_WRONLY|O_CREAT, S_IRUSR | S_IWUSR);
			dup2(fw,1);
		}

		//execvp is removed
		//execvp(command->name, command->args); // exec+args+path

		/// TODO: do your own exec with path resolving using execv()

		//global variable PATH is read and program has gotten all the paths
		char* PATH = getenv("PATH");											

		char *unit_path;
		const char s[2] = ":";
		char *d = "/";
   
		//tokenize path by dividing ":" assigned to s variable.
		//unit_path is the first token 
		unit_path = strtok(PATH, s);
		//it tries all the tokens, unit_path, until it executes successfully.
		while( unit_path != NULL ) {
			char* pathw_cmd = malloc(strlen(unit_path)+strlen(d)+strlen(command->name)+1); 	//memory allocation for char array concat process
			
			strcpy(pathw_cmd, unit_path); 											//first part is copied to pathw_cmd
			strcat(pathw_cmd, d);													//"/" is concatenated
			strcat(pathw_cmd, command->name); 										//command name is concatenated to pathw_cmd
			//printf("looking for command in: %s\n",pathw_cmd);
			if(execv(pathw_cmd,command->args)!=-1){									//if path input in execv includes that command it executes and exits.
				exit(0);
			}										
			free(pathw_cmd);														//memory allocation freed
			unit_path = strtok(NULL, s);											//if execv does not work, it tries next path
		}
	}
	else
	{
    // TODO: implement background processes here
		//It waits for the specific pid, if the process is not background one.
		if((command->background)==false){
			waitpid(pid,NULL,NULL);
		}
		return SUCCESS;
	}

	// TODO: your implementation here

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}


void chatroom_func(char *room_name, char *user_name){
	/*
		this part creates chatroom-room_name folder in the /tmp/ path. 
	*/
	char* cr_path_template = "/tmp/chatroom-";
    
    char* cr_path = malloc(strlen(cr_path_template)+strlen(room_name)+1);
    strcpy(cr_path,cr_path_template);
    strcat(cr_path,room_name);
	//save the initial directory in the case of changing the dir
    char cwd_init[256];
    getcwd(cwd_init, sizeof(cwd_init));
	//if the dir does not exist, it creates the dir
    if(chdir(cr_path)==-1){
        mkdir(cr_path);
    }
    char cwd_poss[256];
    getcwd(cwd_poss, sizeof(cwd_poss));
	//if cwd is changed, return back
    if(strcmp(cwd_init,cwd_poss)){
        chdir(cwd_init);
    }
    
    printf("\nWelcome to %s!\n\n",room_name);

	//named pipe of the registered user is created.
	char *slash = "/";
	char *namedPipe = malloc(strlen(cr_path)+strlen(user_name)+strlen(slash)+1);
	strcpy(namedPipe,cr_path);
	strcat(namedPipe,slash);
	strcat(namedPipe,user_name);
	mkfifo(namedPipe, 0666);
	
	//prev will be used for temp memory of handling new message is gotten or not.
	char prev[MAX_BUF];

    pid_t p;
	p=fork();
	//parent process opens the user's named pipe and continously checks its pipe
	//for new messages. When it confronts new message, it prints.
	if(p==0){
		int fd;
		fd = open(namedPipe, O_RDWR | O_NONBLOCK);
		while(1){
			char *buf = malloc(MAX_BUF);
			if(read(fd, buf, MAX_BUF)>0 && strcmp(prev,buf)!=0){
				printf("[%s] %s\n", room_name,buf);
				strcpy(prev,buf);
			}  
			free(buf);
		}
		close(fd);
		
	}
	//child process waits for the input message of user.
	//when the user writes smt to others, it iterates over files in the room directory and writes
	//the message at each named pipe.
	else{
		while(1){
			char message1[MAX_BUF];
			fgets(message1, MAX_BUF, stdin);
			printf("[%s] %s: %s\n", room_name,user_name,message1);
			char *middle=": ";
			//message includes input and name of the user.
			char *message=malloc(strlen(message1)+strlen(user_name)+strlen(middle)+1);
			strcpy(message,user_name);
			strcat(message,middle);
			strcat(message,message1);
			char *slash="/";
			DIR *dir;
			struct dirent *ent;
			if ((dir = opendir (cr_path)) != NULL) {
				while ((ent = readdir (dir)) != NULL) {
					//if the path includes .. or . pass
					if(strcmp(ent->d_name,".")==0 || strcmp(ent->d_name,"..")==0) continue;
					//pass the named pipe of user itself
					if(strcmp(ent->d_name,user_name)==0) continue;

					char *pipe = malloc(strlen(cr_path)+strlen(ent->d_name)+strlen(slash)+1);
					strcpy(pipe,cr_path);
					strcat(pipe,slash);
					strcat(pipe,ent->d_name);
					//it opens the other user' named pipes and writes on them
					int fd;
					fd = open(pipe, O_RDWR|O_NONBLOCK);
					write(fd, message, strlen(message));
					close(fd);
				}
			} 		
			else {
				perror ("");
				return EXIT_FAILURE;
			}
			closedir (dir);
		}
		
	}
}

///Pipe helper
int pipe_execute(struct command_t *command){
	//Rearrange the arguments to keep the command name in the first index
	command->args = (char**) realloc(command->args,sizeof(char*)*(command->arg_count += 2));
	for(int i = command->arg_count - 2 ; i > 0; --i){
		command -> args[i] = command -> args[i-1];
	}
	command -> args[0] = strdup(command -> name);

	//Create a pipe
    int fd[2];
	if (pipe(fd) == -1) {
		fprintf(stderr,"Pipe failed");
		return EXIT;
	}
	
	//Fork a new child for the first command
    pid_t left = fork();
    if(left == 0){
        close(1); //Close the standart output
        close(fd[0]); //Close the read and of the pipe
        dup2(fd[1],1); //Connect the standart output to write end of the pipe
		close(fd[1]); //Close write end

        //global variable PATH is read and program has gotten all the paths
		char* PATH = getenv("PATH");											
		char *unit_path;
		const char s[2] = ":";
		char *d = "/";
		//tokenize path by dividing ":" assigned to s variable.
		//unit_path is the first token 
		unit_path = strtok(PATH, s);
		//it tries all the tokens, unit_path, until it executes successfully.
		while( unit_path != NULL ) {
			char* pathw_cmd = malloc(strlen(unit_path)+strlen(d)+strlen(command->name)+1); 	//memory allocation for char array concat process
			strcpy(pathw_cmd, unit_path); 											//first part is copied to pathw_cmd
			strcat(pathw_cmd, d);													//"/" is concatenated
			strcat(pathw_cmd, command->name); 										//command name is concatenated to pathw_cmd
			if(execv(pathw_cmd,command->args)!= -1){									//if path input in execv includes that command it executes and exits.
				exit(0);
			}										
			free(pathw_cmd);														//memory allocation freed
			unit_path = strtok(NULL, s);											//if execv does not work, it tries next path
		}
		exit(0);
    }

	//Create a second child for the second command
	pid_t right = fork();
	if(right == 0){
        close(0);
        close(fd[1]);
        dup2(fd[0],0);
		close(fd[0]);
		process_command(command -> next);
		exit(0);
    }

    close( fd[0] );
    close( fd[1] );
    waitpid(left,NULL,NULL);
	waitpid(right,NULL,NULL);
    return SUCCESS;
}

int uniq_func(int flag){
	//Buffer for keeping only one word
	char buffer[60];
	//A keeper array to keep all words 
	char* keeper[100];
	// A unique array to keep unique elements
	char* unique_keeper[100];
	int total_words = 0;
	//Read from the standart input
    while(fgets(buffer, 60 , stdin) != NULL)
    {
		char* tmp = malloc(strlen(buffer));
		strcpy(tmp,buffer);
		//Write into the keeper array
		keeper[total_words] = tmp;
		total_words++;
    }

	keeper[total_words] = NULL;
	//Update the unique array
	int j = 0;
	int k = 0;
	while(keeper[j] != NULL){
		int isElementPresent = 0; 
    	for (int i = 0; i < k; i++) {
			//printf("%s---%s\n",unique_keeper[i],keeper[j]);
			//printf("result of comparison %d\n",strcmp(unique_keeper[i],keeper[j]));
			if (strcmp(unique_keeper[i],keeper[j])==0) {
				isElementPresent = 1;
				break;
			}
    	}
		if(isElementPresent == 0){
			unique_keeper[k] = keeper[j];
			//printf("element of unique j = %d k = %d %s\n",j,k, unique_keeper[k]);
			k++;
		}
		j++;
	}
	unique_keeper[k] = NULL;
	//Print the unique array
	if(flag == 0){
		int l = 0;
		while(unique_keeper[l] != NULL){
			printf("%s",unique_keeper[l]);
			l++;
		} 
	}

	if(flag == 1){
		int i = 0;
		while(unique_keeper[i] != NULL){
			int j = 0;
			int counter = 0;
			while(keeper[j] != NULL){
				if(strcmp(unique_keeper[i],keeper[j]) == 0){
					counter ++;
				}
				j++;
			}
			printf("%d %s",counter,unique_keeper[i]);
			i++;
		}
	}
	return SUCCESS;

}
void vigenere_func(char *_mode, char *input_mes, char *_key){
	char msg[MAX_BUF];
    char key[MAX_BUF];
	strcpy(msg,input_mes);
	strcpy(key,_key);
	int msg_len = strlen(input_mes);
	int key_len = strlen(_key);
	if(msg_len<key_len){
		printf("%s\n","key length can not be longer that message length.");
		exit(0);
	}
	msg[msg_len]='\0';
	key[key_len]='\0';
	//printf("key:%s, plaintext:%s\n",key,msg);

	char pure_msg[MAX_BUF];
	int k;
	int l=0;
	for(k=0;k<msg_len;k++){
		if(isalpha(msg[k])){
			pure_msg[l]=msg[k];
			l++;
		}
	}
	pure_msg[l]='\0';
	msg_len = strlen(pure_msg);

	char longVersionOfKey[msg_len];
	int i;
	int j;
	for(i = 0; i < msg_len; i++){
        longVersionOfKey[i] = key[i%key_len];
    }
	longVersionOfKey[msg_len]='\0';
	//printf("%s\n",longVersionOfKey);

	if(strcmp(_mode,"enc")==0){
		char encOut[msg_len];
		for(i = 0; i < msg_len; ++i){
        	encOut[i] ='A' + ((toupper(pure_msg[i]) + toupper(longVersionOfKey[i])) % 26);
		}
    	encOut[msg_len] = '\0';
		printf("Encrypted message: %s\n",encOut);
	}
	else if(strcmp(_mode,"dec")==0){
		char decOut[msg_len];
		for(i = 0; i < msg_len; ++i){
        	decOut[i] ='A' + (((toupper(pure_msg[i]) - toupper(longVersionOfKey[i])) + 26) % 26);
		}
    	decOut[msg_len] = '\0';
		printf("Decrypted message: %s\n",decOut);
	}
	else{
		printf("%s\n","mode can be enc or dec");
	}
}

int wiseman_function(int min){
	FILE *fp = fopen("crontab_temp", "w");
  	fprintf(fp, "*/%d * * * * fortune | espeak \n", min);
 	fclose(fp);
	struct command_t *command=malloc(sizeof(struct command_t));
	memset(command, 0, sizeof(struct command_t)); // set all bytes to 0
	char m_name[100];
	strcpy(m_name,"crontab");
	command -> name = m_name;
	command ->arg_count = 1;
	command -> args = malloc(sizeof(char**));
	memset(command ->args,0,sizeof(char**));
	char* filename = malloc(20);
	memset(filename, 0, 20);
	strcpy(filename,"crontab_temp");
	command -> args[0] = filename;
	process_command(command);
}

int reflex_func(){
	char * args[2];
	args[0] = "figlet";
	args[1] = "Press Enter to Start";
	pid_t pid = fork();
	if(pid == 0){
		int control = execvp(args[0],args);
		if(control == -1){
			return EXIT;
		}
		exit(0);
	}
	waitpid(pid,NULL,NULL);
	while(getchar() != '\n');
	printf("%s\n\n","Press enter when u see the CAT :)");
	int sleep_time = (rand() % (4 - 1 + 1)) + 1;
	sleep(sleep_time);
	char * cat = "|\\---/|\n| o_o |\n \\_^_/ ";
	printf("%s\n",cat);
	long long initial_time = timeInMilliseconds();
	while(getchar() != '\n');
	long long ending_time = timeInMilliseconds();
	int delta_time = (int) ending_time - initial_time;
	printf("\nreaction time is %d miliseconds\n",delta_time);
	return SUCCESS;
}



long long timeInMilliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}
