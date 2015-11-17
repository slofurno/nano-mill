#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
#include <unistd.h>

#define FANOUT "tcp://127.0.0.1:666"
#define FANIN "tcp://127.0.0.1:667"
#define REPORT "tcp://127.0.0.1:668"

int main(void)
{
    printf("worker started\n");
    
    char msg[100];
    while(1){
        int reporter = nn_socket(AF_SP,NN_PUSH);
        nn_connect(reporter,FANIN);

        int consumer = nn_socket(AF_SP,NN_PULL);
        nn_connect(consumer,FANOUT);
        
        size_t nbytes;
        char *buf = NULL;

        nbytes = nn_recv(consumer,&buf,NN_MSG,0);
        printf("got work for id: %s\n",buf);
        sleep(2);
        printf("work %s done, reporting\n", buf);

        int n = sprintf(msg,"%s|finished :D",buf);
        nn_send(reporter,&msg,n+1,0);
        printf("report sent\n");
        nn_freemsg(buf);
    }    
}
