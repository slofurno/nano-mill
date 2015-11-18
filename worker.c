#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define FANOUT "tcp://127.0.0.1:666"
#define FANIN "tcp://127.0.0.1:667"
#define REPORT "tcp://127.0.0.1:668"

int forkorsomething(char*);

int main(void)
{
    printf("worker started\n");
    
    char msg[100];
    int reporter = nn_socket(AF_SP,NN_PUSH);
    nn_connect(reporter,FANIN);
    int max_sz = -1;
    int consumer = nn_socket(AF_SP,NN_PULL);
    nn_connect(consumer,FANOUT);
    nn_setsockopt(consumer, NN_SOL_SOCKET, NN_RCVMAXSIZE, &max_sz, sizeof(max_sz)); 
    size_t nbytes;
    char *buf = NULL;

    while(1){

        nbytes = nn_recv(consumer,&buf,NN_MSG,0);
        
        char *uuid = malloc(sizeof(char)*37);
        memcpy((void*)uuid,(const void*)buf,36);
        uuid[36]='\0';
        
        char *tevs = malloc(100);
        sprintf(tevs, "tmp/%s",uuid);
        FILE *file = fopen(tevs, "wb");

        fwrite(buf+36, sizeof(char), nbytes-36, file);
        fclose(file);        

        printf("got work for id: %s\n",uuid);
        sleep(2);
        forkorsomething(uuid);
        printf("work %s done, reporting\n", uuid);

        int n = sprintf(msg,"%s|finished :D",uuid);
        nn_send(reporter,msg,n+1,0);
        printf("report sent\n");
    }    
}

int forkorsomething(char *uuid)
{
    int fd[2];
    pipe(fd);
    pid_t childpid;
    char *shellpath = "./tevs.sh";
    size_t sz = strlen(shellpath)+strlen(uuid)+2;
    printf("size : %d\n",sz);
    char *command = malloc(sizeof(char)*sz);
    size_t s_sz = sprintf(command, "%s %s", shellpath, uuid);
    
    printf("printed chars: %d\n",s_sz);
    
    printf("%s\n",command);  

    childpid = fork();
    if (childpid == -1){
       fprintf(stderr, "FORK failed");
       return -1;
    } else if (childpid == 0) {
       close(1);
       dup2(fd[1], 1);
       close(fd[0]);
       execlp("/bin/sh","/bin/sh","-c", command, NULL);
    }

    int nread;
    char buffer[128];

    if(read(fd[0], buffer, 128)==-1) {
            printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
    }
    else{
        printf("ok.. %s\n",buffer);
    }

    wait(NULL); 
    return 0;
}
