#include "kernel/types.h"
#include "user/user.h"

// 了解fork的时候对fd的操作 fd是文件描述符，fork的时候子进程也会拥有自己的fd（意味着文件资源的引用+1），指向父进程打开的文件，如果子进程close(fd)，不影响父进程对文件的读写，
// 但是需要注意的一点是父进程和子进程共享fd的offset!这点很重要，只有当文件的ref为0的时候，文件资源才会被os释放

/***
第一个进程1输入2-35；第二个进程2打印第一个素数2，然后将所有进程1输入的、是2的倍数的数剔除后，剩下的输入给进程3；进程3打印第二个素数3，然后将所有进程2输入的、是3的倍数的数剔除后，剩下的输入给进程4…

例如：打印2-10之间的素数。

    第一个进程往管道里输入2-10
    第二个进程打印2，把2的倍数剔除后，剩下：3，5，7，9
    第三个进程打印3，把3的倍数剔除后，剩下：5，7
    第四个进程打印5，把5的倍数剔除后，剩下：7
    第五个进程打印7，把7的倍数剔除后，没了

总之，一个程序从它的父进程管道中获取的第一个数一定是素数。
***/

// 并发素数筛 好名字

#define N_num 35

int sieve_primes(int* origin, int* sieved, int significant_bits,int exclude){
    int final_significant_bits = 0;
    for (int i=1;i<significant_bits;i++){
        if(*(origin+i)%(exclude) != 0){
            final_significant_bits++;
            *(sieved+final_significant_bits-1) = *(origin+i);
        }
    }
    for (int i=final_significant_bits;i<N_num-1;i++){
        *(sieved+i) = 0;
    }
    //printf("%d \n",final_significant_bits);
    return final_significant_bits;
}

void printarray(int* arraybuf){
    for(int i=0;i<N_num-1;i++){
        printf("%d ",*(arraybuf+i));
    }
    printf("\n");
}

//void primes_pipe(int pid, int* p, int* buf, int significant_bits, int* process_num)
//之前在调用primes函数的时候fork()了,而不是在函数内部,结果read到的数据很奇怪.那样父子进程就不在一个函数里面了,也不在一个内存栈,pipe得到的read就很奇怪...maybe是这个原因
void primes_pipe(int* p, int* buf, int significant_bits){

    int status;
    int p_right[2]; // 如果每次都用这个新管道将导致child和parent的pipe的p不一致.
    pipe(p);
    
    if(fork() == 0){
        close(p[1]);
        int buf_child_read[N_num - 1];
        for(int i=0;i<N_num-1;i++){
            read(p[0],(buf_child_read+i),sizeof(i)); 
        }
        //printarray(buf_child_read);
        //read(p[0],buf_child_read,sizeof(buf_child_read));
        close(p[0]);
        printf("prime %d\n",buf_child_read[0]);

        int final_significant_bits = sieve_primes(buf_child_read,buf,significant_bits,buf_child_read[0]);
        if(final_significant_bits!= 0){
            primes_pipe(p_right,buf,final_significant_bits);   // 灵光一闪想到递归!
        }
        exit(1);
    }
    else{
        close(p[0]);
        for(int i=0;i<N_num-1;i++){
            write(p[1],(buf+i),sizeof(i)); 
        } 
        close(p[1]);
        wait(&status);      
    }

}


int main(int argc, char *argv[]){

    int buf[N_num-1];
    for(int i=2;i<=N_num;i++){
        buf[i-2]=i;
    }

    int p[2];
    int significant_bits = N_num-1;

    primes_pipe(p,buf,significant_bits);

    exit(0);
}