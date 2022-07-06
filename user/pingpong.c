#include "kernel/types.h"
#include "user/user.h"

// pipe is use for the communication between the process
// 我们将用pipe()函数建立管道。每当我们打开数据流时，它都会加入描述符表。
// pipe()函数也是如此，它创建两条相连的数据流，并把它们加到描述符表中，然后只要我们往其中一条数据流中写数据，就可以从另一条数据流中读取。

/***  version1

int
main(int argc, char *argv[])
{   
    int N = 4 ;
    char buf_ping[] = "ping";
    char buf_child_read[16];
    char buf_pong[] = "pong";
    char buf_parent_read[16];

    int pipe_fd_p2c_read_write[2]={1,2};  // the fd=2(write_fd) is the fd in parent or child?
    pipe(pipe_fd_p2c_read_write);
    int pid = fork();
    int status;
    write(1,buf_ping,N);

    if(pid == 0){
        read(2,buf_child_read,N);
        int current_c_pid = getpid();
        printf("%d: received %s",current_c_pid,buf_child_read);

        close(1);
        int pipe_fd_c2p_read_write[2]={1,3}  ;
        pipe(pipe_fd_c2p_read_write);    // 子进程结束会被杀掉    
        write(1,buf_pong,N);
        exit(0);
    }
    else{
        wait(&status);
        read(3,buf_parent_read,N);

        int current_p_pid = getpid();
        printf("%d: received %s",current_p_pid,buf_parent_read);
    }

    exit(0);
}
***/

int
main(int argc, char *argv[])
{   
    int N = 4 ;
    char buf_ping[] = "ping";
    char buf_child_read[16];
    char buf_pong[] = "pong";
    char buf_parent_read[16];

    int pipe_fd_p2c_read_write[2]={0,1};  // the fd=2(write_fd) is the fd in parent or child?
    pipe(pipe_fd_p2c_read_write);
    int pid = fork();
    int status;

    if(pid == 0){
        // close(pipe_fd_p2c_read_write[1]);
        read(pipe_fd_p2c_read_write[0],buf_child_read,N);
        close(pipe_fd_p2c_read_write[0]);   // IMPORTANT!!
        int current_c_pid = getpid();
        //printf("child receive:%s \n",buf_child_read);
        printf("%d: received %s\n",current_c_pid,buf_child_read);
 
        write(pipe_fd_p2c_read_write[1],buf_pong,N);
        close(pipe_fd_p2c_read_write[1]);   // IMPORTANT!!
        exit(1);
    }
    else{
        
        //---the child's pipe.read will wait parent's write------
        write(pipe_fd_p2c_read_write[1],buf_ping,N);
        close(pipe_fd_p2c_read_write[1]);   // IMPORTANT!!
        //-------------------------------------------------------

        wait(&status);
        read(pipe_fd_p2c_read_write[0],buf_parent_read,N);
        close(pipe_fd_p2c_read_write[0]);   // IMPORTANT!!

        int current_p_pid = getpid();
        //printf("parent receive:%s \n",buf_parent_read);
        printf("%d: received %s\n",current_p_pid,buf_parent_read);
    }

    exit(0);
}