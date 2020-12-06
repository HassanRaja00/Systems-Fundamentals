/*
 * Legion: Command-line interface
 */
// #define _GNU_SOURCE
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include "legion.h"
#include "debug.h"

extern char **environ;



int status = 1; //can be changed to 0 if you want to exit loop
char* command;
struct daemons *list;
int daemon_count = 0; //the next open spot
int most_recent_index;
int flag_killed_child = 0;
int flag_error_stop = 0;
int flag_child_exited = 0;

struct daemons {
	char *name;
	pid_t pid;
	int using;
	char *status;
	char *execute;
	char **arg_vector;
} daemons;



char* get_daemon_status(enum daemon_status status) {
	switch(status) {
		case status_unknown: return "unknown";
		case status_inactive: return "inactive";
		case status_starting: return "starting";
		case status_active: return "active";
		case status_stopping: return "stopping";
		case status_exited: return "exited";
		case status_crashed: return "crashed";
	}
	return "unknown";
}


char* get_word_from_args(int *last);
char** get_arg_vector_for_daemon();
void free_daemon_list();
int file_exists(char *fname);

/* this helps catch ^C */
void handle_sigint(int sig) {
	//dont forget to do here what u do with quit!! (once u finish quit)
	free_daemon_list();
	sf_fini();
    exit(0);
}

/* helps catch sigalrm */
void handle_sigalrm(int sig) {
	debug("killing %s with pid %d", list[most_recent_index].name, list[most_recent_index].pid);
	kill(list[most_recent_index].pid, SIGKILL); //kill the child process if it takes > 1 sec
	sf_kill(list[most_recent_index].name, list[most_recent_index].pid);
	//set the child status to what??
	list[most_recent_index].status = get_daemon_status(status_stopping);
	flag_killed_child = 1;
	flag_error_stop = 1;

}

/* helps with SIGCHLD */
void handle_sigchld(int sig) {
	debug("in sigchld handler");

 	int status = 0; //will tell you what status the program exited with
 	int i;
 	// int signal;
 	for(i = 0; i < daemon_count; i++) {
 		if(list[i].using == 1 && !strcmp(list[i].status, "active")) {
 			waitpid(list[i].pid, &status, 0);
 			debug("%s", list[i].name);
 			if(WIFEXITED(status)) { //check exit signals
 				sf_term(list[i].name, list[i].pid, status);
	 			list[i].status = get_daemon_status(status_exited);
	 			flag_child_exited = 1; //for stop
	 			// break;
 			}
 			else {
 				sf_crash(list[i].name, list[i].pid, sig);
	 			list[i].status = get_daemon_status(status_crashed);
	 			flag_error_stop = 1;
	 			// break;
 			}
 		}
 	}


}



void run_cli(FILE *in, FILE *out)
{
    // TO BE IMPLEMENTED
	// int x; //flag for what the first arg is

	signal(SIGINT, handle_sigint);
	signal(SIGALRM, handle_sigalrm);
	signal(SIGCHLD, handle_sigchld);
	sigset_t signal_set; //will only hold sigchld
	sigemptyset(&signal_set);
	sigaddset(&signal_set, SIGCHLD); // sigset = { SIGCHLD }
	sigset_t old_mask;



	list = malloc(sizeof(struct daemons));

    do{
    	// debug("daemon count: %d", daemon_count);
    	if(!status){
    		break;
    	}
    	printf("legion> ");

    	//read in the first word
    	int last_flag = 0;
    	command = get_word_from_args(&last_flag);
    	debug("arg: %s\n\n", command);

    	if(!strcmp(command, "help") && last_flag){
    		printf("\nAvailable commands:\n"
			"help (0 args) Print this help message\n"
			"quit (0 args) Quit the program\n"
			"register (2 args) Register a daemon\n"
			"unregister (1 args) Unregister a daemon\n"
			"status (1 args) Show the status of a daemon\n"
			"status-all (0 args) Show the status of all daemons\n"
			"start (1 args) Start a daemon\n"
			"stop (1 args) Stop a daemon\n"
			"logrotate (1 args) Rotate log files for a daemon\n"
			"\n");
			free(command);
    	}
    	else if(!strcmp(command, "quit") && last_flag){
    		debug("this is quit");
    		// iterate thru list and free all strings, then the list
    		free_daemon_list();
    		free(command);
    		break;
    	}
    	else if(!strcmp(command, "stop") && !last_flag){
    		debug("this is stop");
    		char* name = get_word_from_args(&last_flag);
    		if(!last_flag){ //more than 1 arg
    			while(!last_flag){
    				free(name);
    				name = get_word_from_args(&last_flag);
    			}
    			free(name);
    			// printf("Usage: start <daemon>\n\n");
    			sf_error("Usage: stop <daemon>");
    			continue;
    		}

    		int i, exists = 0;
    		for(i = 0; i < daemon_count; i++) {
    			if(list[i].using == 1 && !strcmp(list[i].name, name)){
    				exists = 1;
    				break;
    			}
    		}
    		free(name);
    		if(!exists){
    			sf_error("Daemon does not exist!");
    			free(command);
    			continue;
    		}

    		if(!strcmp(list[i].status, "exited") || !strcmp(list[i].status, "crashed")) {
    			list[i].status = get_daemon_status(status_inactive);
    			sf_reset(list[i].name);
    			free(command);
    			continue;
    		}
    		if(strcmp(list[i].status, "active")){
    			sf_error("Can only stop an active daemon!");
    			free(command);
    			continue;
    		}
    		most_recent_index = i;
    		list[i].status = get_daemon_status(status_stopping);
    		sf_stop(list[i].name, list[i].pid);
    		sigprocmask(SIG_BLOCK, &signal_set, &old_mask);
    		kill(list[i].pid, SIGTERM);
    		alarm(CHILD_TIMEOUT);
    		sigsuspend(&old_mask); //only a sig child can wake up the program
    		alarm(0); // cancel alarm
    		// sigprocmask(SIG_UNBLOCK, &signal_set, &old_mask);
    		if(flag_error_stop){
    			sf_error("an error occurred in stop");
    			flag_error_stop = 0;
    			free(command);
    			continue;
    		}
    		if(flag_child_exited){

    			flag_child_exited = 0;
    			free(command);
    			continue;
    		}
    		debug("exited the handler");
    		free(command);
    		continue;


    	}
    	else if(!strcmp(command, "start") && !last_flag){
    		debug("this is start");
    		char* name = get_word_from_args(&last_flag);
    		debug("%s", name);
    		if(!last_flag){ //more than 1 arg
    			while(!last_flag){
    				free(name);
    				name = get_word_from_args(&last_flag);
    			}
    			free(name);
    			// printf("Usage: start <daemon>\n\n");
    			sf_error("Usage: start <daemon>");
    			free(command);
    			continue;
    		}
    		int i;
    		int exists = 0;
    		for(i = 0; i < daemon_count; i++) {
    			if(list[i].using == 1 && !strcmp(list[i].name, name)){
    				exists = 1;
    				break;
    			}
    		}
    		free(name);

    		if(!exists){
    			sf_error("Daemon does not exist!");
    			free(command);
    			continue;
    		}
    		if(strcmp(list[i].status, "inactive")){
    			sf_error("Daemon not inactive, error!");
    			free(command);
    			continue;
    		}

    		// debug("found a daemon in start");
    		//list[i] will be used now
    		list[i].status = get_daemon_status(status_starting);

    		int pipefd[2]; // 0 -> read end | 1 -> write end
    		pid_t cpid;
    		if(pipe(pipefd) == -1){
				sf_error("error in pipe");
				list[i].status = get_daemon_status(status_inactive);
				free(command);
				continue;

			}
			//call sigprogmask before forking
			sigprocmask(SIG_BLOCK, &signal_set, NULL);

			cpid = fork();
			sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
			if(cpid == -1) {
				sf_error("error in forking child!");
				//set the daemon status to inactive
				list[i].status = get_daemon_status(status_inactive);
				free(command);
			}
			else if(cpid> 0){
				//parent process
				debug("parent");

				close(pipefd[1]); //close writing end of pipe
				list[i].pid = cpid;
				most_recent_index = i;
				char c;
				//unmask alarm signal
				flag_killed_child = 0;
				alarm(CHILD_TIMEOUT); //how to only wait for the 1-bit message???
				int a = read(pipefd[0], &c, 1); //read the one bit sync message
				if(a >= 0){
					debug("cancelling alarm");
					alarm(0);//cancel alarm
				}
				list[i].status = get_daemon_status(status_active);
				sf_active(list[i].name, cpid);
				close(pipefd[0]);

			}
			else {
				//child process
				debug("child");
				close(pipefd[0]); //close reading end of pipe
				char *env_name = getenv(PATH_ENV_VAR);
				char d[8] = "daemons:";
				strcat(d, env_name);
				setenv(PATH_ENV_VAR, d, 1);

				char log_name[7] = ".log.0\0";
				char name[strlen(list[i].name)];
				strcpy(name,list[i].name);

				strcat(name, log_name);
				char dir[6] = "logs/\0";
				strcat(dir, name);
				// debug("%s", dir);
				char d_dir[9] = "daemons/\0";
				char execute[strlen(list[i].execute)];
				strcpy(execute, list[i].execute);
				strcat(d_dir, execute);
				// debug("%s", d_dir);

				// debug("%s,%s", list[i].name, list[i].execute);

				//write to daemonNAME.log.0
				FILE *log = fopen(dir, "w+");

				dup2(pipefd[1], SYNC_FD);

				//printf will go to this log file
				freopen(dir, "w+", stdout);
				//do commands for lazy
				setpgid(cpid, cpid); //join its own process group
				// debug("setpgid");
				sf_start(list[i].name);
				debug("before execvpe");
				execvpe(d_dir, list[i].arg_vector, environ);
				debug("execvpe done");

				fclose(log);
				close(pipefd[1]);//close the writing end

			}
			sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
			free(command);
			continue;

    	}
    	else if(!strcmp(command, "status") && !last_flag){
    		debug("this is status");
    		free(command);
    		char* name = get_word_from_args(&last_flag);
    		if(!last_flag){
    			while(!last_flag){
    				free(name);
    				name = get_word_from_args(&last_flag);
    			}
    			free(name);
    			// printf("Usage: status <daemon>\n\n");
    			sf_error("Usage: status <daemon>");
    			continue;
    		}
    		int i;
    		int exists = 0;
    		for(i = 0; i < daemon_count; i++) {
    			if(list[i].using == 1 && !strcmp(list[i].name, name)){
    				exists = 1;
    				break;
    			}
    		}

    		if(!exists){
    			printf("unknown\n\n");
    		}
    		else{
    			printf("%s\t%d\t%s\n", list[i].name, list[i].pid, list[i].status);
    		}
    		free(name);
    		continue;

    	}
    	else if(!strcmp(command, "register") && !last_flag){
    		debug("this is register");
    		free(command);
    		char* name = get_word_from_args(&last_flag);
    		if(last_flag) {
    			free(command);
    			free(name);
    			// printf("Usage: register <daemon> <cmd-and-args>\n\n");
    			sf_error("Usage: register <daemon> <cmd-and-args>");
    			continue; //go to next iteration
    		}
    		int exists = 0;
    		for(int i =0; i < daemon_count; i++){
    			if(!strcmp(list[i].name, name) && list[i].using == 1){
    				sf_error("Daemon already exists!");
    				exists = 1;
    				break;
    			}
    		}
    		char* executable = get_word_from_args(&last_flag);

    		if(last_flag){
    			if(exists){
    				free(command);
	    			free(name);
	    			free(executable);
	    			continue;
	    		}
    			//enter the daemon as is
    			// char *arg[1];
    			// arg[0] = "\0";
    			list[daemon_count].name = name;
    			list[daemon_count].pid = 0;
    			list[daemon_count].using = 1;
    			list[daemon_count].status = get_daemon_status(status_inactive);
    			list[daemon_count].execute = executable;
    			list[daemon_count].arg_vector = NULL;
    			daemon_count++;
    			list = realloc(list, sizeof(struct daemons) * (daemon_count+1));
    			debug("successful add");
    			sf_register(name, executable);
    			continue;

    		}
    		//get arg_vector
    		char **arg_vector = get_arg_vector_for_daemon();
    		if(exists){ //u gotta clear the input
    			free(command);
    			free(name);
    			free(executable);
    			free(arg_vector);
	    		continue;
	    	}
    		list[daemon_count].name = name;
			list[daemon_count].pid = 0;
			list[daemon_count].using = 1;
			list[daemon_count].status = get_daemon_status(status_inactive);
			list[daemon_count].execute = executable;
			list[daemon_count].arg_vector = arg_vector;
			daemon_count++;
			list = realloc(list, sizeof(struct daemons) * (daemon_count+1));
			debug("successful add");
			sf_register(name, executable);
			continue;



    	}
    	else if(!strcmp(command, "status-all") && last_flag){
    		debug("this is status-all");
    		free(command);
    		for(int i = 0; i < daemon_count; i++) {
    			if(list[i].using == 1){
    				printf("%s\t%d\t%s\n", list[i].name, list[i].pid, list[i].status);
    			}
    		}
    		continue;
    	}
    	else if(!strcmp(command, "unregister") && !last_flag){
    		debug("this is unregister");
    		free(command);
    		char* name = get_word_from_args(&last_flag);
    		if(!last_flag) {
    			free(command);
    			free(name);
    			// printf("Usage: unregister <daemon>\n\n");
    			sf_error("Usage: unregister <daemon>");
    			continue; //go to next iteration
    		}
    		int exists = 0;
    		int i;
    		for(i = 0; i < daemon_count; i++){
    			if(!strcmp(list[i].name, name) && list[i].using == 1){
    				exists = 1;
    				break;
    			}
    		}
    		if(!exists){
    			sf_error("This daemon does not exist!");
    			free(name);
    			continue;
    		}
    		if(strcmp(list[i].status, "inactive")){
    			sf_error("This daemon is not in the inactive state!");
    			free(name);
    			continue;
    		}
    		list[i].using = 0;
    		sf_unregister(name);
    		free(name);
    		debug("successful removal");
    		continue;
    	}
    	else if(!strcmp(command, "logrotate") && !last_flag){
    		debug("this is logrotate");
    		free(command);
    		int last_flag = 0;
    		char *name = get_word_from_args(&last_flag);
    		if(!last_flag){
    			while(!last_flag){
    				free(name);
    				name = get_word_from_args(&last_flag);
    			}
    			free(name);
    			sf_error("Usage: logrotate <daemon>");
    			continue;
    		}

    		int exists = 0;
    		int i;
    		for(i = 0; i < daemon_count; i++){
    			if(!strcmp(list[i].name, name) && list[i].using == 1){
    				exists = 1;
    				break;
    			}
    		}
    		if(!exists){
    			sf_error("This daemon does not exist!");
    			free(name);
    			continue;
    		}
    		free(name);

    		char path[6] = "logs/\0";
    		char file_ext[7] = ".log.0\0";
    		char fname[strlen(list[i].name)];
    		strcpy(fname, list[i].name);
    		strcat(fname, file_ext);
    		strcat(path, fname);
    		//check if the file exists with the corresponding daemon name
    		if(!file_exists(path)){
    			sf_error("Cannot rotate a log file that does not exist");
    			continue;

    		}
    		//check if the daemon is active
    		int d_stopped = 0;
    		if(!strcmp(list[i].status, "active")){
    			//if its active stop the daemon first
    			d_stopped = 1;
    			most_recent_index = i;
	    		list[i].status = get_daemon_status(status_stopping);
	    		sf_stop(list[i].name, list[i].pid);
	    		sigprocmask(SIG_BLOCK, &signal_set, &old_mask);
	    		kill(list[i].pid, SIGTERM);
	    		alarm(CHILD_TIMEOUT);
	    		sigsuspend(&old_mask); //only a sig child can wake up the program
	    		alarm(0); // cancel alarm
	    		// sigprocmask(SIG_UNBLOCK, &signal_set, &old_mask);
	    		if(flag_error_stop){
	    			sf_error("an error occurred in stop");
	    		}

    		}

    		char digits[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    		int highest_log = 1;
    		char l_version;
    		for(int j = 1; j < LOG_VERSIONS; j++) {
    			l_version = digits[highest_log];
    			char lpath[6] = "logs/\0";
    			char l_ext[7] = {'.', 'l', 'o', 'g', '.', l_version, '\0'};
    			// debug("%s", l_ext);
    			char lname[strlen(list[i].name)];
    			strcpy(lname, list[i].name);

    			strcat(lname, l_ext);
    			strcat(lpath, lname);
    			// debug("%s", lpath);
    			char p_version = digits[highest_log-1];
    			char p_path[6] = "logs/\0";
    			char p_ext[7] = {'.', 'l', 'o', 'g', '.', p_version, '\0'};
    			char pname[strlen(list[i].name)];
    			strcpy(pname, list[i].name);
    			strcat(pname, p_ext);
    			strcat(p_path, pname);
    			if(!file_exists(lpath)) {
    				rename(p_path, lpath);
    				break;
    			}
    			if(j == (LOG_VERSIONS -1)) {
    				unlink(lpath);
    			}


    			// debug("%s, %s", p_path, lpath);
    			// rename(p_path, lpath);
    			highest_log++;
    		}
    		sf_logrotate(list[i].name);

    		if(d_stopped == 1){
    			//restart daemon
    			list[i].status = get_daemon_status(status_starting);

	    		int pipefd[2]; // 0 -> read end | 1 -> write end
	    		pid_t cpid;
	    		if(pipe(pipefd) == -1){
					sf_error("error in pipe");
					list[i].status = get_daemon_status(status_inactive);
					continue;

				}
	    			//call sigprogmask before forking
				sigprocmask(SIG_BLOCK, &signal_set, NULL);

				cpid = fork();
				sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
				if(cpid == -1) {
					sf_error("error in forking child!");
					//set the daemon status to inactive
					list[i].status = get_daemon_status(status_inactive);
				}
				else if(cpid> 0){
					//parent process
					debug("parent");

					close(pipefd[1]); //close writing end of pipe
					list[i].pid = cpid;
					most_recent_index = i;
					char c;
					//unmask alarm signal
					flag_killed_child = 0;
					alarm(CHILD_TIMEOUT); //how to only wait for the 1-bit message???
					int a = read(pipefd[0], &c, 1); //read the one bit sync message
					if(a >= 0){
						debug("cancelling alarm");
						alarm(0);//cancel alarm
					}
					list[i].status = get_daemon_status(status_active);
					sf_active(list[i].name, cpid);
					close(pipefd[0]);

				}
				else {
					//child process
					debug("child");
					close(pipefd[0]); //close reading end of pipe
					char *env_name = getenv(PATH_ENV_VAR);
					char d[8] = "daemons:";
					strcat(d, env_name);
					setenv(PATH_ENV_VAR, d, 1);

					char log_name[7] = ".log.0\0";
					char name[strlen(list[i].name)];
					strcpy(name,list[i].name);

					strcat(name, log_name);
					char dir[6] = "logs/\0";
					strcat(dir, name);
					// debug("%s", dir);
					char d_dir[9] = "daemons/\0";
					char execute[strlen(list[i].execute)];
					strcpy(execute, list[i].execute);
					strcat(d_dir, execute);
					// debug("%s", d_dir);

					// debug("%s,%s", list[i].name, list[i].execute);

					//write to daemonNAME.log.0
					FILE *log = fopen(dir, "w+");

					dup2(pipefd[1], SYNC_FD);

					//printf will go to this log file
					freopen(dir, "w+", stdout);
					//do commands for lazy
					setpgid(cpid, cpid); //join its own process group
					// debug("setpgid");
					sf_start(list[i].name);
					debug("before execvpe");
					execvpe(d_dir, list[i].arg_vector, environ);
					debug("execvpe done");

					fclose(log);
					close(pipefd[1]);//close the writing end

				}
				sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
	    		}



    	}
    	else {
    		// printf("Error executing command!\n");
    		sf_error("Error executing command!");
    		free(command);
    		if(!last_flag) {
    			char *c;
    			while(!last_flag){
    				c = get_word_from_args(&last_flag);
    				free(c);
    			}
    		}
    	}


    } while(status);
}

char* get_word_from_args(int *last_flag){
	char *buff;
	char c = getchar();
	int char_count = 1;

	if(c == 39) {
		 buff = malloc(sizeof(char));
		//skip the '
		c = getchar();
		while(1){
			buff = realloc(buff, char_count+1);
			if(c == 39 || c== '\n'){
				//add \0 and return buff
				buff[char_count-1] = '\0';
				if(c == '\n'){
					*last_flag = 1;
				}
				else{
					c = getchar();//skip the space after
					if(c == '\n'){ // if instead of a space it's \n
						*last_flag = 1;
					}
				}
				return buff;
			}
			if((int)c==EOF){
				//free all the stuff
				free(buff);
				free_daemon_list();
				sf_fini();
				exit(0);
			}

			buff[char_count-1] = c;
			char_count++;
			c = getchar();

		}

	}

	buff = malloc(sizeof(char));
	while(1) {
		buff = realloc(buff, char_count+1);
		if(c == ' ' || c == '\n'){
			// add \0 and return that char
			buff[char_count-1] = '\0';
			if(c == '\n'){
				*last_flag = 1;
			}

			return buff;
		}
		if((int)c==EOF){
			//free all the stuff
			free(buff);
			free_daemon_list();
			sf_fini();
			exit(0);
		}

		buff[char_count-1] = c;
		char_count++;
		c = getchar();
	}
}

char** get_arg_vector_for_daemon() {
	char **arg_vector = malloc(sizeof(char*));
	int total_size = sizeof(char*);
	int index = 0;
	int last = 0;

	while(1){
		char *w = get_word_from_args(&last);
		total_size += strlen(w);
		arg_vector = realloc(arg_vector, total_size);
		if(last){
			//add last word and return char**
			arg_vector[index] = w;
			return arg_vector;
		}

		arg_vector[index] = w;
		debug("%s", arg_vector[index]);
		index++;

	}

}

void free_daemon_list(){
	for(int i = 0; i< daemon_count; i++){ //free everything you malloc'd
		free(list[i].name);
		free(list[i].execute);
		if(list[i].arg_vector != NULL){
			size_t size = sizeof(*list[i].arg_vector) / sizeof(list[i].arg_vector[0]);
			for(int j = 0; j <= size; j++){
				free(list[i].arg_vector[j]);
			}
			free(list[i].arg_vector);
		}
	}
	free(list);
}

int file_exists(char *fname) {
	FILE *file;
	if((file = fopen(fname, "r"))){
		fclose(file);
		return 1;
	}
	else return 0;
}
