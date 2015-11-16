#include <stdlib.h>
#include <stdio.h>
#include <libmill.h>
#include <string.h>

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

coroutine void handle_conn(tcpsock conn){
    char buf[4096];
    char *header = "Content-Length: ";
    int header_len = strlen(header);
    size_t nbytes; 
    int content_length;
    int n = 0;
    char *method;
    char *url;

    while ((nbytes = tcprecvuntil(conn, buf, 256, "\n" , 1, -1))>0){
        buf[nbytes]='\0';
        printf("rec: %d; %s",nbytes, buf);
        if (n==0){
            int first, second, i;
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
            printf("is this your method?, %s, %s\n", method, url); 
        }
        else if (nbytes>= header_len && strncmp(buf, header, header_len)==0){
            char *rawlen = buf+header_len;
            content_length = atoi(rawlen);
            printf("body size: %d\n",content_length);
        }else if(nbytes==2){
            break;
        }
        n++;
    }

    char *okres ="HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    nbytes = tcpsend(conn, okres, strlen(okres)+1, -1);
    tcpflush(conn, -1);
    printf("wrote response\n"); 
    printf("looking for content length: %d\n",content_length);

    char *file = malloc(content_length);
    char *filep = file;
    size_t left = content_length;
    while(left>0){
        nbytes = tcprecv(conn, filep, left, -1);
        filep+=nbytes; 
        left-=nbytes;
        printf("%d bytes left\n", left);
    }

    //dump_bytes(file, content_length);
    printf("file: \n");
    tcpclose(conn);
    free(file);
}


int main(void){

    ipaddr addr = iplocal(NULL, 80, 0);
    tcpsock ls = tcplisten(addr, 10);
    
    while(1){
        tcpsock s = tcpaccept(ls, -1);
        go(handle_conn(s));
    }
    return 0;
}
