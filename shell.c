#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

#define MAX_ARG_LEN 10
#define MAX_ARG_NUM 80
#define MAX_CMD_LEN 200
#define MAX_CMD_NUM 30

struct Cmd{
    char args[MAX_ARG_NUM][MAX_ARG_LEN];
    char *argps[MAX_ARG_NUM];
    int arg_num;
    char connector; //  how to connect the next cmd, one of `;`, `&`, `|`
    
    char redirect_in[MAX_ARG_LEN], redirect_out[MAX_ARG_LEN];  // save redirection objects
    /*
        no redirection: -1
        redirect stdin: 0
        redirect stdout:1
    */
    int redirect_flag;
} cmds[2][MAX_CMD_NUM];
int cmd_num[2];

char input[2][MAX_CMD_LEN];

int choice; // indicate which one is current cmd's data
int is_first = 1;
int pipe_ind = -1;   // assuming only one pipe has been used

void init(){
    for(int i=0; i<MAX_CMD_NUM; i++){
        cmds[choice][i].redirect_flag = -1;
        memset(cmds[choice][i].redirect_in, 0, MAX_ARG_LEN);
        memset(cmds[choice][i].redirect_out, 0, MAX_ARG_LEN);
        memset(cmds[choice][i].argps, 0, sizeof(char*)*MAX_ARG_NUM);
        memset(cmds[choice][i].args, 0, MAX_ARG_LEN*MAX_ARG_NUM);
    }
}

/*
    prompt and get-parse input line, return 1 if obvious invalid, else return 0
*/
int get_input_and_parse(){
    init();
    printf("osh> ");
    gets(input[choice]);
    int i_arg = 0, i_cmd = 0;
    char *arg = strtok(input[choice], " ");
    int no_connector_end = 0;
    while(arg){
        no_connector_end = 0;
        if(strcmp(arg, "<") == 0){
            cmds[choice][i_cmd].redirect_flag = 0;
            arg = strtok(NULL, " ");
            strcpy(cmds[choice][i_cmd].redirect_in, arg);
            arg = strtok(NULL, " ");
            no_connector_end = 1;
            continue;
        }
        if(strcmp(arg, ">") == 0){
            cmds[choice][i_cmd].redirect_flag = 1;
            arg = strtok(NULL, " ");
            strcpy(cmds[choice][i_cmd].redirect_out, arg);
            arg = strtok(NULL, " ");
            no_connector_end = 1;
            continue;
        }
        if(strcmp(arg, ";") == 0 || strcmp(arg, "&") == 0 || strcmp(arg, "|") == 0){
            cmds[choice][i_cmd].connector = arg[0];

            cmds[choice][i_cmd].arg_num = i_arg;
            // cmds[choice][i_cmd].args[i_arg] = NULL;
            i_arg = 0;

            i_cmd++;
            arg = strtok(NULL, " ");
            continue;
        }
        
        strcpy(cmds[choice][i_cmd].args[i_arg], arg);
        
        arg = strtok(NULL, " ");
        ++i_arg;
        no_connector_end = 1;
    }
    if(no_connector_end){
        cmds[choice][i_cmd].arg_num = i_arg;
        // cmds[choice][i_cmd].args[i_arg] = NULL;
        i_cmd++;
    }
    cmd_num[choice] = i_cmd;

    if(i_cmd == 0){
        return 1;
    }
    return 0;
}

/*
    replace ind-th cmd `!!` with the last cmd list
*/
void expandHistory(int ind){
    assert(cmd_num[choice]+cmd_num[choice^1] < MAX_CMD_NUM);
    int width = cmd_num[choice^1];
    for(int i=cmd_num[choice]-1; i>ind; i--){
        memcpy(&cmds[choice][i+width-1], &cmds[choice][i], sizeof(struct Cmd));
    }
    for(int i=0; i<width; i++){
        memcpy(&cmds[choice][ind+i], &cmds[choice^1][i], sizeof(struct Cmd));
    }
    cmd_num[choice] += width-1;
}

void generate_argps(int i_cmd){
    for(int i=0; i<cmds[choice][i_cmd].arg_num; i++){
        cmds[choice][i_cmd].argps[i] = cmds[choice][i_cmd].args[i];
    }
    cmds[choice][i_cmd].argps[cmds[choice][i_cmd].arg_num] = NULL;
}

/*
    excute 2 cmds connected by `|`
*/
void do_pipe_cmd(int i_cmd){
    int fd[2], pid[2];
    pipe(fd);
    pid[0] = fork();
    if(pid[0] == 0){
        close(fd[0]);
        dup2(fd[1], 1);
        close(fd[1]);
        execvp(cmds[choice][i_cmd].args[0], cmds[choice][i_cmd].argps);
    }

    pid[1] = fork();
    if(pid[1] == 0){
        close(fd[1]);
        dup2(fd[0], 0);
        close(fd[0]);
        execvp(cmds[choice][i_cmd+1].args[0], cmds[choice][i_cmd+1].argps);
    }

    close(fd[0]);
    close(fd[1]);
}

int main(){
    int running = 1;
    while(running){
        int ret = get_input_and_parse();
        if(ret){
            // printf("invalid input\n");
            continue;
        }
        
        int no_expand = 0;  // all `!!` have been expanded with the last cmd list
        while(!no_expand){
            no_expand = 1;
            for(int i_cmd = 0; i_cmd<cmd_num[choice]; i_cmd++){
                if(cmds[choice][i_cmd].arg_num == 1 && strcmp(cmds[choice][i_cmd].args[0], "!!") == 0){
                    // if input `!!`, then use data of last command
                    // if this is the first cmd, then fail
                    if(is_first){
                        printf("No commands in history.\n");
                        continue;
                    }
                    expandHistory(i_cmd);
                    no_expand = 0;
                    break;
                }
            }
        }

        for(int i_cmd = 0; i_cmd<cmd_num[choice]; i_cmd++){
            generate_argps(i_cmd);
        }

        for(int i_cmd = 0; i_cmd<cmd_num[choice]; i_cmd++){
            
            int save_fd, in, out;
            if(cmds[choice][i_cmd].redirect_flag != -1){
                
                if(cmds[choice][i_cmd].redirect_flag == 0){
                    save_fd = dup(0);
                    in = open(cmds[choice][i_cmd].redirect_in, O_RDONLY);
                    dup2(in, 0);
                }else{
                    save_fd = dup(1);
                    out = open(cmds[choice][i_cmd].redirect_out, O_WRONLY|O_CREAT|O_TRUNC, 0666);
                    dup2(out, 1);
                }
            }

            if(cmds[choice][i_cmd].connector == '|'){
                do_pipe_cmd(i_cmd);
                i_cmd += 1;
            }else{

                int pid = fork();
                if(pid == 0){
                    execvp(cmds[choice][i_cmd].args[0], cmds[choice][i_cmd].argps);
                }else if(pid>0){
                    if(cmds[choice][i_cmd].connector != '&'){
                        waitpid(pid, NULL, 0);
                    }
                    // printf("%d\n", pid);
                }
            }

            fflush(stdout);
            // recover stdin & stdout
            if(cmds[choice][i_cmd].redirect_flag == 0){
                dup2(save_fd, 0);
                close(in);
            }else if(cmds[choice][i_cmd].redirect_flag == 1){
                dup2(save_fd, 1);
                close(out);
            }
        }

        choice ^= 1;
        is_first = 0;
    }

}