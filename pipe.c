#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

int main(){

    int fd[2];
    pipe(fd);
    pid_t childpid;
    childpid = fork();

    if (childpid == -1){
       fprintf(stderr, "FORK failed");
       return 1;
    } else if (childpid == 0) {
       close(1);
       dup2(fd[1], 1);
       close(fd[0]);
       execlp("/bin/sh","/bin/sh","-c", "./tevs.sh",NULL);
    }
    wait(NULL);
    char buffer [80];
    size_t count;
    int n;
    n = read(fd[0], buffer, sizeof(buffer));
    buffer[n]='\0';
    printf("%s\n",buffer);

    return 0;
}
