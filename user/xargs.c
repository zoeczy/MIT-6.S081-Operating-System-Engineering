#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"

//实现将标准输入作为参数一起输入到xargs后面跟的命令中
//如果标准输入有多行，那么也要执行多次命令. 例如标准输入中包括"\n"

// void main(int argc, char *argv[]){

//     char *cmd = argv[1];
//     char *arg_for_cmd[argc];
//     for(int i=2; i<argc; i++)   // 最后一个是从标准输入来的
//         arg_for_cmd[i-2]= argv[i];

//     int p[2];
//     pipe(p);
    
//     if(fork()==0){
//         close(p[1]);
//         for(int i=0; i<MAXARG; i++){
//             char arg_from_stand_in[MAXPATH];
//             if(read(p[0],arg_from_stand_in,sizeof(arg_from_stand_in)) != 0){
//                 arg_for_cmd[argc - 1] = arg_from_stand_in;
//                 arg_for_cmd[argc] = "0" ; 
//                 printf("%s \n",arg_from_stand_in);
//                 exec(cmd,arg_for_cmd);
//             }
//              else{break;}
//         }
//         close(p[0]);
//         exit(1);
//     }
//     else{
//         close(p[0]);
//         char arg_from_stand_in[MAXPATH];
//         for(int i=0; i<MAXARG; i++){
//             if( read(0,arg_from_stand_in,sizeof(arg_from_stand_in)) != 0){
//                 write(p[1],arg_from_stand_in,sizeof(arg_from_stand_in));
//             }
//             else{break;}
//         }
//         //printf("%s \n",arg_from_stand_in);
//         close(p[1]);
//         wait((int *) 1);
//     }
// }


// 看了下别人的代码 标准输入没有必要用管道给子进程.不知道为啥我这个管道怎么总写错啊
// 好菜啊我,救......


void main(int argc, char *argv[]){

    char *arg_for_cmd[MAXARG];
    memset(arg_for_cmd,0,sizeof(arg_for_cmd)); // ?set 0 ?
    for(int i=1; i<argc; i++){  // 最后一个是从标准输入来的
        arg_for_cmd[i-1]= argv[i]; // the first arg should be grep cmd !!
        //printf("should be grep and hello:%s  \n",arg_for_cmd[i-1]);
    }
    char *p ;

    //-----SOLUTION FOR ‘p’ may be used uninitialized in this function----
    char buf[512];
    memset(buf,0,sizeof(buf));
    while(1){
        p = buf;    
    //--------------------------------------------------------------------

        char c;
        while( read(0,&c,sizeof(c)) && c != '\n'){
            *p = c;
            p++;
        }

        *p = '\0';

        arg_for_cmd[argc-1] = buf;
        printf("%s \n",arg_for_cmd[argc-1]);
        //arg_for_cmd[argc] = "0";  //if use 0 for the end flag,thr grep will say:"grep: cannot open 0"

        if(fork() == 0){
            exec(argv[1],arg_for_cmd);
            exit(0);
        }
        else{
            wait((int *)0);
            if(p==buf){break;}
        }

    }

}

