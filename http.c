#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <libmill.h>
#include <string.h>
#include <uuid/uuid.h>
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>

#define FANOUT "tcp://127.0.0.1:666"
#define FANIN "tcp://127.0.0.1:667"
#define REPORT "tcp://127.0.0.1:668"

typedef struct slice slice;
struct slice {
    int len;
    char *bytes;
};

typedef struct gif_request gif_request;
struct gif_request {
    slice *uuid;
    slice *data;
};

int min(int n, int m){
    if (n >= m){
        return m;
    } 
    
    return n;  
}


void dump_bytes(char *bytes, int len){
    int i = 0;
    while(i<len){
        printf("%c",bytes[i]);
        i++;
    }

}

coroutine void subscribe(gif_request *request)
{
    int sub = nn_socket(AF_SP,NN_SUB);
    assert(sub>=0);

    char *buf= NULL;
    assert(nn_setsockopt(sub,NN_SUB,NN_SUB_SUBSCRIBE,
                request->uuid->bytes, request->uuid->len-1)>=0);

    assert(nn_connect(sub,REPORT)>=0);

    int fd;
    size_t fd_sz = sizeof(fd);

    assert(nn_getsockopt(sub,NN_SOL_SOCKET,NN_RCVFD,&fd,&fd_sz)>=0);

    printf("waiting for job to finish\n");
    int events = fdwait(fd,FDW_IN,-1);
    if (events & FDW_IN){
        printf("fd ready to read..\n");
    }else{
        printf("fd error??\n");
    }

    size_t nbytes;
    nbytes = nn_recv(sub,&buf,NN_MSG,NN_DONTWAIT);
    printf("rec %d bytes\n",nbytes);
    printf("job done: %s\n",buf);
}


coroutine void start_collector(){

    int publisher = nn_socket(AF_SP,NN_PUB);
    nn_bind(publisher,REPORT);

    int collector = nn_socket(AF_SP,NN_PULL);
    assert(collector>=0);
    assert(nn_bind(collector,FANIN)>=0);

    printf("collector started\n");
    int fd;
    size_t fd_sz = sizeof(fd);

    int send_fd;
    size_t send_fd_sz = sizeof(send_fd);

    assert(nn_getsockopt(publisher,NN_SOL_SOCKET,NN_SNDFD,&send_fd,&send_fd_sz)>=0);
    assert(nn_getsockopt(collector,NN_SOL_SOCKET,NN_RCVFD,&fd,&fd_sz)>=0);
    //char *buf = NULL;
    char buf[100];

    size_t nbytes;

    while(1){
        
        fdwait(fd,FDW_IN,-1);
        nbytes = nn_recv(collector,&buf,sizeof(buf),NN_DONTWAIT);
        printf("lets announce job is done: %s\n",buf);
        
//        fdwait(send_fd,FDW_IN,-1);
        nn_send(publisher,&buf,nbytes+1, NN_DONTWAIT);
       // nn_freemsg(buf);
    }
}

coroutine void start_producer(chan queue)
{
    int producer = nn_socket(AF_SP,NN_PUSH);
    assert(producer>=0);
    assert(nn_bind(producer,FANOUT)>=0);
    
    int fd;
    size_t fd_sz = sizeof(fd);
    size_t bytes;

    assert(nn_getsockopt(producer,NN_SOL_SOCKET,NN_SNDFD,&fd,&fd_sz)>=0);
    //TODO: currently leaking the request upload
    while(1){
        printf("waiting for next request\n");
        gif_request *request= chr(queue,gif_request*);
        printf("producing request id: %s\n", request->uuid->bytes);    

        fdwait(fd,FDW_IN,-1);
        bytes = nn_send(producer,request->uuid->bytes,request->uuid->len,NN_DONTWAIT);
        assert(bytes>0);
    }
}

coroutine void handle_conn(tcpsock conn, chan queue){
    char buf[4096];
    char *header = "Content-Length: ";
    size_t header_len = strlen(header);
    size_t nbytes; 
    int content_length;
    int n = 0;
    char *method;
    char *url;

    while ((nbytes = tcprecvuntil(conn, buf, 256, "\n" , 1, -1))>0){
        buf[nbytes]='\0';
        //printf("rec: %d; %s",nbytes, buf);
        if (n==0){
            size_t first, second, i;
            first=0;
            second=0;

            for(i=0;i<nbytes;i++){
                if (buf[i]=='\x20'){
                    if (first==0){
                        first = i;
                    }else{
                        second = i;
                    }
                }
            }
            method = malloc(first+1);
            url = malloc(second-first+1);  
            sprintf(method, "%.*s", first, buf);
            sprintf(url, "%.*s", second-first, buf+first+1);
            //printf("is this your method?, %s, %s\n", method, url); 
        }
        else if (nbytes>= header_len && strncmp(buf, header, header_len)==0){
            char *rawlen = buf+header_len;
            content_length = atoi(rawlen);
            //printf("body size: %d\n",content_length);
        }else if(nbytes==2){
            break;
        }
        n++;
    }

    char *okres ="HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    nbytes = tcpsend(conn, okres, strlen(okres)+1, -1);
    tcpflush(conn, -1);
    //printf("wrote response\n"); 
    //printf("looking for content length: %d\n",content_length);

    char *file = malloc(content_length);
    char *filep = file;
    size_t left = content_length;
    while(left>0){
        nbytes = tcprecv(conn, filep, left, -1);
        filep+=nbytes; 
        left-=nbytes;
        //printf("%d bytes left\n", left);
    }

    uuid_t uuid;
    char *u = malloc(37);
    
    uuid_generate(uuid);
    uuid_unparse(uuid, u);

    gif_request *request = malloc(sizeof(gif_request));
    slice *data = malloc(sizeof(slice));
    data->bytes = file;
    data->len =content_length; 

    slice *id = malloc(sizeof(slice));
    id->bytes=u;
    id->len=37;
    request->data = data;
    request->uuid = id;

    chs(queue,gif_request*, request);
    //subscribe(request);
    printf("we know our works done\n");
    tcpclose(conn);
    //dump_bytes(file, content_length);
}


int main(void){

    printf("running http listener...\n");
    ipaddr addr = iplocal(NULL, 80, 0);
    tcpsock ls = tcplisten(addr, 10);
    chan queue = chmake(gif_request*, 64);
    
    go(start_producer(queue));
    go(start_collector());

    while(1){
        tcpsock s = tcpaccept(ls, -1);
        go(handle_conn(s, queue));
    }
    return 0;
}
