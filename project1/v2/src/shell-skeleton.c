#include <fcntl.h> 
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

//Completed by Roya Arkh.
void kuhex(const char *file_path, int group_size, FILE *output_stream);
void psvis_command(const char *pid, const char *output_file);
void autocomplete(const char *input, char *buffer, size_t *index, int *tab_count);
int is_duplicate(char matches[][4096], int match_count, const char *new_match);
void list_cd(const char *buffer);

const char *sysname = "dash";

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

//module
void psvis_command(const char *pid, const char *output_file) {
    FILE *proc_write, *proc_read, *output;
    char buffer[1024];

    proc_write = fopen("/proc/psvis_tree","w");

    if (!proc_write) {
        perror("error opening /proc/psvis_tree for writing");
        return;
    }

    fprintf(proc_write, "%s", pid);
    fclose(proc_write);
    proc_read = fopen("/proc/psvis_tree", "r");

    if (!proc_read) {
        perror("error opening /proc/psvis_tree for reading");
        return;
    }

    output = fopen(output_file,"w");
    if (!output) {
        perror("error for opening output file for writing");
        fclose(proc_read);
        return;
    }
    while (fgets(buffer, sizeof(buffer), proc_read)) {
        fputs(buffer, output);
    }

    fclose(proc_read);
    fclose(output);
    printf("Process tree written to %s\n",output_file);
}


/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command) {
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n",
		   command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");

	for (i = 0; i < 3; i++) {
		printf("\t\t%d: %s\n", i,
			   command->redirects[i] ? command->redirects[i] : "N/A");
	}

	printf("\tArguments (%d):\n", command->arg_count);

	for (i = 0; i < command->arg_count; ++i) {
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	}

	if (command->next) {
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command) {
	if (command->arg_count) {
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}

	for (int i = 0; i < 3; ++i) {
		if (command->redirects[i])
			free(command->redirects[i]);
	}

	if (command->next) {
		free_command(command->next);
		command->next = NULL;
	}

	free(command->name);
	free(command);
	return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt() {
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s> ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command) {
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);

	// trim left whitespace
	while (len > 0 && strchr(splitters, buf[0]) != NULL) {
		buf++;
		len--;
	}

	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL) {
		// trim right whitespace
		buf[--len] = 0;
	}

	// auto-complete
	if (len > 0 && buf[len - 1] == '?') {
		command->auto_complete = true;
	}

	// background
	if (len > 0 && buf[len - 1] == '&') {
		command->background = true;
	}

	char *pch = strtok(buf, splitters);
	if (pch == NULL) {
		command->name = (char *)malloc(1);
		command->name[0] = 0;
	} else {
		command->name = (char *)malloc(strlen(pch) + 1);
		strcpy(command->name, pch);
	}

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;

	while (1) {
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		if (len == 0) {
			continue;
		}
		while (len > 0 && strchr(splitters, arg[0]) != NULL) {
			arg++;
			len--;
		}
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL) {
			arg[--len] = 0;
		}
		if (len == 0) {
			continue;
		}

		if (strcmp(arg, "|") == 0) {
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0];
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; 

			parse_command(pch + index, c);
			pch[l] = 0;
			command->next = c;
			continue;
		}

		if (strcmp(arg, "&") == 0) {
			continue;
		}

		redirect_index = -1;
		if (strcmp(arg, "<") == 0) {
    		redirect_index = 0;
		} else if (strcmp(arg, ">") == 0) {
    		redirect_index = 1;
		} else if (strcmp(arg, ">>") == 0) {
    		redirect_index = 2;
		}

				 if (redirect_index != -1) {
            pch = strtok(NULL, splitters);
            if (!pch) {
                fprintf(stderr, "-%s: missing file name for redirection\n", sysname);
                return -1; 
            }
            command->redirects[redirect_index] = strdup(pch);
            continue;
        }

        if (pch[0] == '<') {
            redirect_index = 0;
        } else if (pch[0] == '>') {
            if (pch[1] == '>') {
                redirect_index = 2;
                pch++;
            } else {
                redirect_index = 1;
            }
        }

        if (redirect_index != -1) {
            if (strlen(pch) > 1) {
                command->redirects[redirect_index] = strdup(pch + 1);
            } else {
                pch = strtok(NULL, splitters);
                if (!pch) {
                    fprintf(stderr, "-%s: missing file name for redirection\n", sysname);
                    return -1;
                }
                command->redirects[redirect_index] = strdup(pch);
            }
            continue;
        }


		if (len > 2 &&
			((arg[0] == '"' && arg[len - 1] == '"') ||
			 (arg[0] == '\'' && arg[len - 1] == '\''))) 
		{
			arg[--len] = 0;
			arg++;
		}

		command->args =
			(char **)realloc(command->args, sizeof(char *) * (arg_index + 1));

		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;

	// increase args size by 2
	command->args = (char **)realloc(
		command->args, sizeof(char *) * (command->arg_count += 2));

	// shift everything forward by 1
	for (int i = command->arg_count - 2; i > 0; --i) {
		command->args[i] = command->args[i - 1];
	}

	// set args[0] as a copy of name
	command->args[0] = strdup(command->name);

	// set args[arg_count-1] (last) to NULL
	command->args[command->arg_count - 1] = NULL;

	return 0;
}

void prompt_backspace() {
	putchar(8); // go back 1
	putchar(' '); // write empty over
	putchar(8); // go back 1 again
}

//autocomplete
void autocomplete(const char *input, char *buffer, size_t *index, int *tab_count) {
    struct dirent *entry;
    DIR *dp;
    char *path_env, *token, prefix[4096];
    char matches[1024][4096];
    int match_count = 0, i, j;
    strncpy(prefix, input, 4096);

    (*tab_count)++;

    if (*tab_count > 1) {
        list_cd(buffer);
        return;
    }
    memset(matches, 0, sizeof(matches));

    path_env = getenv("PATH");
    if (!path_env) path_env = "/bin:/usr/bin";

    token = strtok(strdup(path_env), ":");
    while (token) {
        dp = opendir(token);
        if (dp) {
            while ((entry = readdir(dp)) != NULL) {
                if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
                    if (!is_duplicate(matches, match_count, entry->d_name)) {
                        strncpy(matches[match_count++], entry->d_name, 4096);
                        if (match_count >= 1024) break;
                    }
                }
            }
            closedir(dp);
        }
        token = strtok(NULL, ":");
    }

    dp = opendir(".");
    if (dp) {
        while ((entry = readdir(dp)) != NULL) {
            if (strncmp(entry->d_name, prefix, strlen(prefix)) == 0) {
                if (!is_duplicate(matches, match_count, entry->d_name)) {
                    strncpy(matches[match_count++], entry->d_name, 4096);
                    if (match_count >= 1024) break;
                }
            }
        }
        closedir(dp);
    }

    if (match_count == 0) {
        printf("\nNo matches found.\n%s> %s", sysname, buffer);
    } else if (match_count == 1) {
        strncpy(buffer, matches[0], 4096);
        *index = strlen(buffer);
        printf("\n%s> %s", sysname, buffer);
    } else {
        int prefix_len = strlen(matches[0]);
        for (i = 1; i < match_count; i++) {
            for (j = 0; j < prefix_len; j++) {
                if (matches[0][j] != matches[i][j]) {
                    prefix_len = j;
                    break;
                }
            }
        }
        if (prefix_len > (int)strlen(prefix)) {
            strncpy(buffer, matches[0], prefix_len);
            buffer[prefix_len] = '\0';
            *index = strlen(buffer);
        }

        printf("\nPossible matches:\n");
        for (i = 0; i < match_count; i++) {
            printf("%s  ", matches[i]);
        }
        printf("\n%s> %s", sysname, buffer);
    }
}

//helper
int is_duplicate(char matches[][4096], int match_count, const char *new_match) {
    for (int i = 0; i < match_count; i++) {
        if (strcmp(matches[i], new_match) == 0) {
            return 1;
        }
    }
    return 0;
}

//listing current directory
void list_cd(const char *buffer) {
    struct dirent *entry;
    DIR *dp = opendir(".");
    if (!dp) return;

    printf("\nFiles in current directory:\n");
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] == '.') {// for hidden files to not show
            continue;
        }
        printf("%s  ", entry->d_name);
    }
    closedir(dp);
    printf("\n%s> %s", sysname, buffer);
}

int prompt(struct command_t *command) {
    size_t index = 0;
    char c;
    char buf[4096];
    static char oldbuf[4096];
    int tab_count = 0;

    static struct termios backup_termios, new_termios;
    tcgetattr(STDIN_FILENO, &backup_termios);
    new_termios = backup_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    show_prompt();
    buf[0] = 0;

    while (1) {
        c = getchar();
		if (c == 9) {
            autocomplete(buf, buf, &index, &tab_count);
            continue;
        }
        tab_count = 0;
        if (c == 127) {
            if (index > 0) {
                prompt_backspace();
                index--;
                buf[index] = '\0';
            }
            continue;
        }
		if (c == 27 || c == 91 || c == 66 || c == 67 || c == 68) {
			continue;
		}
		if (c == 65) {
			while (index > 0) {
				prompt_backspace();
				index--;
			}

			char tmpbuf[4096];
			printf("%s", oldbuf);
			strcpy(tmpbuf, buf);
			strcpy(buf, oldbuf);
			strcpy(oldbuf, tmpbuf);
			index += strlen(buf);
			continue;
		}

        putchar(c);
        buf[index++] = c;
        if (index >= sizeof(buf) - 1) break;
        if (c == '\n') break;
        if (c == 4) return EXIT;
    }

    buf[--index] = '\0';
    strcpy(oldbuf, buf);

    parse_command(buf, command);
    tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
    return SUCCESS;
}

int process_command(struct command_t *command);

int main() {
	while (1) {
		struct command_t *command = malloc(sizeof(struct command_t));

		memset(command, 0, sizeof(struct command_t));

		int code;
		code = prompt(command);
		if (code == EXIT) {
			break;
		}
		code = process_command(command);
		if (code == EXIT) {
			break;
		}

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command) {
    int fd_in = STDIN_FILENO;
    int pipe_fd[2];
    struct command_t *current = command;

    if (strcmp(command->name, "psvis") == 0) {
    if (command->arg_count < 3) {
        fprintf(stderr, "Usage: psvis <PID> <output file>\n");
        return SUCCESS;
    }
    const char *pid = command->args[1];
    const char *output_file = command->args[2];
  
    FILE *proc_file = fopen("/proc/psvis_tree", "w");
    if (!proc_file) {
        perror("Error opening /proc/psvis_tree for writing");
        return SUCCESS;
    }
    fprintf(proc_file, "%s\n", pid);
    fclose(proc_file);

    proc_file = fopen("/proc/psvis_tree", "r");
    if (!proc_file) {
        perror("Error opening /proc/psvis_tree for reading");
        return SUCCESS;
    }

    FILE *dot_file = fopen(output_file, "w");
    if (!dot_file) {
        perror("error creating output file");
        fclose(proc_file);
        return SUCCESS;
    }

    char buffer[4096];
    while (fgets(buffer,sizeof(buffer), proc_file)) {
        fputs(buffer,dot_file);
    }
    printf("Process tree written to %s\n", output_file);
    fclose(proc_file);
    fclose(dot_file);
    return SUCCESS;
    }
	
	if (strcmp(command->name,"exit")==0) {
        return EXIT;
    }

    if (strcmp(command->name,"cd")==0) {
        const char *dir=(command->arg_count>1) ? command->args[1]:getenv("HOME");
        if (dir == NULL){
            fprintf(stderr, "-%s: cd: HOME is not set!\n", sysname);
        } else if (chdir(dir)==-1) {
            fprintf(stderr, "-%s: cd: %s: %s\n", sysname, dir, strerror(errno));
        }
        return SUCCESS;
    }

	if (strcmp(command->name, "kuhex") == 0) {
    if (command->arg_count < 2) {
        fprintf(stderr, "Usage: kuhex <file> [-g group_size]\n");
        return SUCCESS;
    }

    const char *file_path = command->args[1];
    int group_size = 1;
    if (command->arg_count > 3 && strcmp(command->args[2], "-g") == 0) {
        group_size = atoi(command->args[3]);
        if (group_size <= 0) {
            fprintf(stderr, "invalid group size: %s\n", command->args[3]);
            return SUCCESS;
        }
    }

    FILE *output_stream = stdout;
    if (command->redirects[1]) { 
        output_stream = fopen(command->redirects[1],"w");
        if (!output_stream) {
            perror("error opening output file");
            return SUCCESS;
        }
    } else if (command->redirects[2]) {
        output_stream = fopen(command->redirects[2],"a");
        if (!output_stream) {
            perror("error opening output file");
            return SUCCESS;
        }
    }
    kuhex(file_path, group_size, output_stream);
    if (output_stream != stdout) {
        fclose(output_stream);
    }
    return SUCCESS;
	}

    while (current) {
        pipe(pipe_fd);

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork error!");
            return UNKNOWN;
        }

        if (pid == 0) {
            if (fd_in != STDIN_FILENO) { 
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            if (current->next) { 
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
            }
            if (current->redirects[0]) {
                int fd = open(current->redirects[0], O_RDONLY);
                if (fd < 0) {
                    perror("error for opening input file");
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (current->redirects[1]) {
                int fd = open(current->redirects[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("error opening output file");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            if (current->redirects[2]) {
                int fd = open(current->redirects[2], O_WRONLY | O_CREAT | O_APPEND, 0644);
                if (fd < 0) {
                    perror("error opening append output file");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            close(pipe_fd[0]);

            if (strchr(current->name, '/')) {
                execv(current->name, current->args);
            } else {
                char *path_env = getenv("PATH");
                if (!path_env) path_env = "/bin:/usr/bin";
                char *path_copy = strdup(path_env);
                char *token = strtok(path_copy, ":");
                while (token) {
                    char fullpath[1024];
                    snprintf(fullpath, sizeof(fullpath), "%s/%s", token, current->name);
                    if (access(fullpath, X_OK) == 0) {
                        execv(fullpath, current->args);
                    }
                    token = strtok(NULL, ":");
                }
                free(path_copy);
            }
            perror("command execution failed");
            exit(1);
        } else {
			if (command->background) {
                printf("[%d] %s\n", pid, current->name);
                close(pipe_fd[1]);
                fd_in = pipe_fd[0];
            } else {
                close(pipe_fd[1]);
                waitpid(pid, NULL,0);
                fd_in = pipe_fd[0];
            }
            current = current->next;
        }
    }
    return SUCCESS;
}


void kuhex(const char *file_path, int group_size, FILE *output_stream) {
    if (group_size != 1 && group_size != 2 && group_size != 4 &&
        group_size != 8 && group_size != 16) {
        fprintf(stderr, "invalid group size, supported sizes: 1,2,4,8,16.\n"); 
        return;
    }

    FILE *file = fopen(file_path,"rb");
    if (!file) {
        perror("error openin the file");
        return;
    }

    unsigned char buffer[16];
    size_t bytes_read;
    size_t offset = 0;

    while ((bytes_read=fread(buffer,1,16,file))>0) {
        fprintf(output_stream, "%08lx: ", offset);
        offset += bytes_read;
        for (size_t i = 0;i<16; i+=(size_t)group_size) {
            if (i < bytes_read) {
                for (size_t j = 0; j < (size_t)group_size; j++) {
                    if (i + j < bytes_read) {
                        fprintf(output_stream,"%02x",buffer[i + j]);
                    } else {
                        fprintf(output_stream,"  ");
                    }
                }
                fprintf(output_stream," ");
            } else {
                for (size_t j=0;j<(size_t)(group_size *2+1);j++) {
                    fprintf(output_stream, " ");
                }
            }
        }
        fprintf(output_stream, " ");
        for (size_t i=0;i<bytes_read; i++) {
            fprintf(output_stream,"%c",isprint(buffer[i])?buffer[i]:'.');
        }
        fprintf(output_stream, "\n");
    }
    fclose(file);
}
