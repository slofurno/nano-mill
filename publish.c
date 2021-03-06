#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>
#include <libmill.h>

coroutine void produce(char *url){
    int producer = nn_socket(AF_SP, NN_PUSH);
    assert(producer>=0);
    assert(nn_bind(producer, url)>=0);
    int bytes; 
    char msg [50];
    printf("producer ok\n");
    
    int fd;
    size_t fd_sz = sizeof(fd);

    if (nn_getsockopt(producer, NN_SOL_SOCKET, NN_SNDFD, &fd, &fd_sz)<0){
       printf("failed to get send fd with errno %d\n",nn_errno());
    }

    int n;
    int i = 20;
    while(--i >=0){
       n = sprintf(msg, "item %d",i);

       fdwait(fd, FDW_IN, -1);
       bytes = nn_send(producer, &msg, n+1,NN_DONTWAIT);
       
       assert(bytes>=0);

       if (bytes<0){
            printf("nn_send failed: %s\n",nn_strerror(nn_errno()));
       }
       printf("produced job %d, send %d bytes\n",i,bytes);
       msleep(now()+500);
    }    
}

coroutine void consume(char *url, int id){
    printf("starting worker %d\n",id);
    int consumer = nn_socket(AF_SP, NN_PULL);
    assert(consumer>=0);
    assert(nn_connect(consumer,url)>=0);

    int fd;
    size_t fd_sz = sizeof(fd);

    if (nn_getsockopt(consumer, NN_SOL_SOCKET, NN_RCVFD, &fd, &fd_sz)<0){
       printf("failed to set opt with errno %d\n",nn_errno());
    }
        
    char *buf= NULL;
    printf("worker started: %d\n", id);

    while(1){
        fdwait(fd, FDW_IN, -1);
        assert(nn_recv(consumer, &buf, NN_MSG, NN_DONTWAIT)>=0);

        printf("worker %d got a job: %s\n",id, buf);
        nn_freemsg(buf);
        msleep(now()+2000);
    }

}

coroutine void publish(){

    int pub = nn_socket (AF_SP, NN_PUB);
    assert(pub>=0); 
    assert(nn_bind(pub,"tcp://127.0.0.1:666")>=0);
    int nbytes;
    while(1){
        char *d ="yo man";
        int sz_d = strlen(d) + 1;
        nbytes = nn_send (pub, d, sz_d, NN_DONTWAIT);
        printf("publishing hello\n");
        msleep(now()+10000);
    }
}

coroutine void subscribe(){
    printf("wtf m8");
    int sub = nn_socket (AF_SP, NN_SUB);
    assert( sub>= 0);
    int n;
    
    char *buf = NULL;
    printf("subbing on fd %d\n",sub);
    assert (nn_setsockopt (sub, NN_SUB, NN_SUB_SUBSCRIBE, "", 0) >= 0);
    assert(nn_connect(sub,"tcp://127.0.0.1:666")>=0);

    int fd;
    size_t fd_sz = sizeof(fd);

    if (nn_getsockopt(sub, NN_SOL_SOCKET, NN_RCVFD, &fd, &fd_sz)<0){
        printf("failed to set opt with errno %d\n",nn_errno());
    }

    printf("wait on fd %d\n",fd);

    while(1){
        fdwait(fd, FDW_IN, -1);
        printf("rdy to rec?\n");
        assert(nn_recv(sub, &buf, NN_MSG, NN_DONTWAIT)>=0);

        printf("rec: %s\n",buf);
        nn_freemsg(buf);
    }
}

int main (const int argc, const char **argv)
{
    //go(publish());
    char *url = "tcp://127.0.0.1:667";
    
    go(produce(url));
    msleep(now()+3000);
    go(consume(url, 1));
    go(consume(url, 2));
    go(consume(url, 3));
    msleep(now()+120000);
    while(0){
        //go(subscribe());
        //msleep(now()+10000); 
    }
    return 0; 
}
