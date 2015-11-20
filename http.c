#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <libmill.h>
#include <string.h>
#include <uuid/uuid.h>
#include <nanomsg/nn.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>
#include <nanomsg/reqrep.h>

#include "slice.c"

#define REPORT "tcp://127.0.0.1:668"
#define WORKERROUTER "tcp://*:666"
#define REC_SIZE 500000000


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
    int events;
    int nbytes=-1;
    
    //so fd isnt signaled when this subscriber has data...
    //its signaled anytime ANY subscriber has data, for now
    //ill just assume if nbytes==-1, errno = EAGAIN
    do{
        events = fdwait(fd,FDW_IN,-1);
        if (events & FDW_IN){
            printf("fd %d for sub fd %d signaled..\n", fd, sub);
        }else{
            printf("fd error??\n");
        }
        nbytes = nn_recv(sub,&buf,NN_MSG,NN_DONTWAIT);
    }while(nbytes==-1);

    if (nbytes>0){
        printf("rec %d bytes\n",nbytes);
        printf("job done: %s\n",buf);
    }else if(nbytes==0){
        printf("how do we rec 0 bytes??\n");
    }else{
        printf("error : %s\n",nn_strerror(nn_errno()));
    }

    nn_close(sub);
}


coroutine void start_collector(chan workers, const int worker_router){

    int publisher = nn_socket(AF_SP, NN_PUB);
    nn_bind(publisher, REPORT);

    printf("collector started\n");
    int fd;
    size_t fd_sz = sizeof(fd);

    assert(nn_getsockopt(worker_router, NN_SOL_SOCKET, NN_RCVFD, &fd, &fd_sz) >= 0);

    char *body = malloc(sizeof(char)*REC_SIZE);
    char uuid[37];
    char directory[70];

    struct nn_msghdr hdr;

    while(1){
        char *ctrl = malloc(sizeof(char)*64);
        memset(&hdr, 0, sizeof(hdr));

        struct nn_iovec iovec;
        iovec.iov_base = body;
        iovec.iov_len = REC_SIZE;

        hdr.msg_iov = &iovec;
        hdr.msg_iovlen = 1;
        hdr.msg_control = ctrl;
        hdr.msg_controllen = 64;

        int events = fdwait(fd, FDW_IN, -1);

        events = fdwait(fd,FDW_IN,-1);
        if (events & FDW_IN){
            printf("fd d for sub fd d signaled..\n");
        }else{
            printf("fd error??\n");
        }
        
        printf("worker coming in\n");
        int rc = nn_recvmsg(worker_router, &hdr, NN_DONTWAIT);
        assert(rc>=0);
        printf("fuck yuou\n");

        printf("msg is: %.*s\n", rc, (char*)hdr.msg_iov->iov_base);
        
        if (rc >= 100){
            memcpy(uuid, body, sizeof(char)*36);
            sprintf(directory, "tmp/REAL-%s.gif", uuid);
            printf("lets announce job is done: %s\n", uuid);

            FILE *f = fopen(directory, "w");
            fwrite(body+36, sizeof(char), rc-36, f);
            fclose(f);

            nn_send(publisher, uuid, 36, NN_DONTWAIT);
        }

        chs(workers, char*, ctrl);
    }
}

coroutine void start_router(chan workers, int worker_router, chan jobs)
{
    void *buf = NULL;

    while(1){
        printf("waiting for next request\n");
        gif_request *job = chr(jobs, gif_request*);
        printf("literally, what?\n");
        printf("producing request id: %s\n", job->uuid->bytes);    

        size_t sum = job->uuid->len-1 + job->data->len;
        
        printf("allocating %d\n",sum);
        buf = nn_allocmsg (sum, 0);
       // buf = malloc(sizeof(char)*sum);
        assert(buf != NULL);

        memcpy(buf, job->uuid->bytes, 36);
        memcpy(buf+36, job->data->bytes, job->data->len);

        free_slice(job->data);

        char *worker_header = chr(workers, char*);

        struct nn_msghdr hdr;
        memset(&hdr, 0, sizeof(hdr));

        hdr.msg_control = worker_header;
        hdr.msg_controllen = 64; 

        struct nn_iovec iovec;
        iovec.iov_base = &buf;
        iovec.iov_len = NN_MSG;

        hdr.msg_iov = &iovec;
        hdr.msg_iovlen = 1;

        printf("about to send work\n");
        nn_sendmsg(worker_router, &hdr, NN_DONTWAIT);
        printf("work sent\n");
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


int main(int argc, char **argv){

    if (argc>1){
        printf("%s\n",argv[1]);
    }

    printf("running http listener...\n");
    ipaddr addr = iplocal(NULL, 80, 0);
    tcpsock ls = tcplisten(addr, 10);

    chan jobs = chmake(gif_request*, 64);
    chan workers = chmake(char*, 64);

    int rcv_max = -1;

    int worker_router = nn_socket(AF_SP_RAW, NN_REP);
    assert(worker_router >= 0);

    nn_bind(worker_router, WORKERROUTER);
    assert(nn_setsockopt(worker_router, NN_SOL_SOCKET, 
                NN_RCVMAXSIZE, &rcv_max, sizeof(rcv_max)) >= 0);
    printf("options set...\n");

    go(start_router(chdup(workers), worker_router, chdup(jobs)));
    printf("router started\n");

    go(start_collector((workers), worker_router));
    printf("skipping collector?\n");

    while(1){
        tcpsock s = tcpaccept(ls, -1);
        go(handle_conn(s, chdup(jobs)));
    }
    return 0;
}
