#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <libmill.h>
#include <string.h>
#include <uuid/uuid.h>
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>
#include "slice.c"

#define FANOUT "tcp://*:666"
#define FANIN "tcp://*:667"
#define REPORT "tcp://127.0.0.1:668"


typedef struct gif_request gif_request;
struct gif_request {
    slice *uuid;
    slice *data;
};

void free_gif_request(gif_request *s)
{
    free_slice(s->uuid);
    free_slice(s->data);
}

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
    printf("sub on fd: %d\n",sub);
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
        printf("fd %d for sub fd %d signaled..\n", fd, sub);
    }else{
        printf("fd error??\n");
    }

    int nbytes;
    nbytes = nn_recv(sub,&buf,NN_MSG,NN_DONTWAIT);
    if (nbytes>0){
        printf("rec %d bytes\n",nbytes);
        printf("job done: %s\n",buf);
    }else if(nbytes==0){
        printf("how do we rec 0 bytes??\n");
    }else{
        printf("error : %s\n",nn_strerror(nn_errno()));
    }
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
    int max_sz = -1;

    assert(nn_getsockopt(publisher,NN_SOL_SOCKET,NN_SNDFD,&send_fd,&send_fd_sz)>=0);
    assert(nn_getsockopt(collector,NN_SOL_SOCKET,NN_RCVFD,&fd,&fd_sz)>=0);
    nn_setsockopt(collector,NN_SOL_SOCKET,NN_RCVMAXSIZE,&max_sz,sizeof(max_sz));

    char *buf = NULL;
    size_t nbytes;
    char uuid[37];
    char directory[70];

    while(1){
        
        fdwait(fd,FDW_IN,-1);
        nbytes = nn_recv(collector,&buf,NN_MSG,NN_DONTWAIT);

        memcpy(uuid,buf,sizeof(char)*36);
        sprintf(directory,"tmp/REAL-%s.gif",uuid);
        printf("lets announce job is done: %s\n",uuid);

        FILE *f = fopen(directory, "w");
        fwrite(buf+36, sizeof(char), nbytes-36,f);
        fclose(f);
        
//        fdwait(send_fd,FDW_IN,-1);
        nn_send(publisher,uuid,36, NN_DONTWAIT);
        nn_freemsg(buf);
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

    //is 100mb enough queue size for uploads?
    int snd_buf_len = 100000000;
    void *buf = NULL;

    assert(nn_getsockopt(producer,NN_SOL_SOCKET,NN_SNDFD,&fd,&fd_sz)>=0);
    assert(nn_setsockopt(producer,NN_SOL_SOCKET,NN_SNDBUF,&snd_buf_len,sizeof(snd_buf_len))>=0);
    //TODO: currently leaking the request upload
    while(1){
        printf("waiting for next request\n");
        gif_request *request= chr(queue,gif_request*);
        printf("producing request id: %s\n", request->uuid->bytes);    

        size_t sum = request->uuid->len-1 + request->data->len;
        
        printf("allocating %d\n",sum);
        buf = nn_allocmsg (sum, 0);
       // buf = malloc(sizeof(char)*sum);
        assert(buf!=NULL);
        void *bp = buf+36;

        memcpy(buf, request->uuid->bytes,36);
        memcpy(bp,request->data->bytes,request->data->len);

        free_slice(request->data);
        //TODO:still using uuid in subscribe
        //free_gif_request(request);
        fdwait(fd,FDW_IN,-1);
//        bytes = nn_send(producer,request->uuid->bytes,request->uuid->len,NN_DONTWAIT);
        bytes = nn_send(producer, &buf, NN_MSG, NN_DONTWAIT);
        assert(bytes==sum);
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


    uuid_t uuid;
    char *u = malloc(37);
    uuid_generate(uuid);
    uuid_unparse(uuid, u);

    char *okres ="HTTP/1.1 200 OK\r\nContent-Length: 36\r\nContent-Type: text/plain; charset=utf-8\r\n\r\n";
    nbytes = tcpsend(conn, okres, strlen(okres)+1, -1);
    tcpsend(conn, u, strlen(u), -1);
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
    go(subscribe(request));
    tcpclose(conn);
    //dump_bytes(file, content_length);
}


int main(void){

    printf("running http listener...\n");
    ipaddr addr = iplocal(NULL, 80, 0);
    tcpsock ls = tcplisten(addr, 10);
    chan queue = chmake(gif_request*, 64);
    
    go(start_producer(chdup(queue)));
    go(start_collector());

    while(1){
        tcpsock s = tcpaccept(ls, -1);
        go(handle_conn(s, chdup(queue)));
    }
    return 0;
}
