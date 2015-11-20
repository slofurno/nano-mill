#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
#include <nanomsg/reqrep.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "slice.c"

#define WORKERROUTER "tcp://127.0.0.1:666"

slice* forkorsomething(char*);

int main(void)
{
    printf("what: %d\n", ENOPROTOOPT);
    printf("worker started\n");
    int max_sz = -1;
    int rsnd = 2000000000;
    
    int router = nn_socket(AF_SP, NN_REQ);
    nn_setsockopt(router, NN_SOL_SOCKET, NN_RCVMAXSIZE, &max_sz, sizeof(max_sz));
    int optres = nn_setsockopt(router, NN_REQ, NN_REQ_RESEND_IVL, &rsnd, sizeof(rsnd)); 
    printf("whats this: %d\n", optres);
    assert(optres==0);
    nn_connect(router, WORKERROUTER);

    size_t nbytes;
    char *buf = NULL;

    char *greeting = "hey, another worker is here!";
    nn_send(router, greeting, strlen(greeting), 0); 

    while(1){
    
        nbytes = nn_recv(router, &buf, NN_MSG, 0);
        
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
        slice *result = forkorsomething(uuid);
        printf("work %s done, reporting\n", uuid);

        nn_send(router, result->bytes, result->len+1, 0);
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
