#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <libmill.h>

coroutine void publish(){

    int pub = nn_socket (AF_SP, NN_PUB);
//    nn_setsockopt(pub, 
    assert(nn_bind(pub,"tcp://127.0.0.1:666")>=0);
    int nbytes;
    printf("ARE WE PUBBING\n"); 
    assert(pub>=0); 
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
    int maxwait = 10000;
    size_t sz = sizeof(maxwait);
    int sub = nn_socket (AF_SP, NN_SUB);
//    nn_getsockopt(sub, NN_SUB, NN_RCVTIMEO, &maxwait, &sz);
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
        //int events = fdwait(sub, FDW_IN, -1);
        fdwait(fd, FDW_IN, -1);
        printf("rdy to rec?\n");
        n = nn_recv(sub, &buf, NN_MSG, NN_DONTWAIT);
        
        if (n<0){
            printf("WE WERENT READy??");
            msleep(now()+1000); 
        }else{
            printf("rec: %s\n",buf);
            nn_freemsg(buf);
        }
    }
}

int main (const int argc, const char **argv)
{
    printf("wtf man...\n");
    go(publish());
    while(1){
        go(subscribe());
        msleep(now()+10000); 
    }
    return 0; 
}
