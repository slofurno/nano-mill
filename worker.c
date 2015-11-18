#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "slice.c"

#define FANOUT "tcp://127.0.0.1:666"
#define FANIN "tcp://127.0.0.1:667"
#define REPORT "tcp://127.0.0.1:668"

slice* forkorsomething(char*);

int main(void)
{
    printf("worker started\n");
    
    int reporter = nn_socket(AF_SP,NN_PUSH);
    nn_connect(reporter,FANIN);
    int max_sz = -1;
    size_t nbytes;
    char *buf = NULL;

    while(1){
        int consumer = nn_socket(AF_SP,NN_PULL);
        nn_connect(consumer,FANOUT);
        nn_setsockopt(consumer, NN_SOL_SOCKET, NN_RCVMAXSIZE, &max_sz, sizeof(max_sz)); 
    
        nbytes = nn_recv(consumer,&buf,NN_MSG,0);
        nn_close(consumer);
        
        char *uuid = malloc(sizeof(char)*37);
        memcpy((void*)uuid,(const void*)buf,36);
        uuid[36]='\0';
        
        char *tevs = malloc(100);
        sprintf(tevs, "tmp/%s",uuid);
        FILE *file = fopen(tevs, "wb");

        fwrite(buf+36, sizeof(char), nbytes-36, file);
        fclose(file);        
        nn_freemsg(buf);

        printf("got work for id: %s\n",uuid);
        sleep(2);
        slice *result = forkorsomething(uuid);
        printf("work %s done, reporting\n", uuid);

        nn_send(reporter,result->bytes,result->len +1,0);
        free_slice(result);
        printf("report sent\n");
    }    
}

slice* forkorsomething(char *uuid)
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
       return NULL;
    } else if (childpid == 0) {
       close(1);
       dup2(fd[1], 1);
       close(fd[0]);
       execlp("/bin/sh","/bin/sh","-c", command, NULL);
    }

    close(fd[1]);
    int nread;
    char buffer[4096];
    slice *result = make_slice();
    append(result,uuid); 

    while((nread=read(fd[0], buffer, 4096))>0) {
        appendn(result,buffer,nread); 
    }

    printf("read %d bytes from result\n",result->len);
//    wait(NULL); 
    return result;
}
