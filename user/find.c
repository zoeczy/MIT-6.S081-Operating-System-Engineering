#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void match(const char* path, const char* name){
    //printf("%s %s \n", path, name);
    int pp = 0;
    int pa = 0;
    while(path[pp] != 0){
        pa = 0;
        int np = pp;
        while(name[pa] != 0){
            if (name[pa] == path[np]){
                pa++;
                np++;
            }
            else
                break;
        }
        if(name[pa] == 0){
            printf("%s\n", path);
            return;
        }
        pp++;
    }
}

void
find(char *path, char *name)
{
  char buf[512],*p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:
    match(path,name); 
    break;

  case T_DIR:
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0) continue;
        if(de.name[0] == '.' && de.name[1] == 0) continue;
        if(de.name[0] == '.' && de.name[1] == '.' && de.name[2] == 0) continue;
        memmove(p, de.name, DIRSIZ); //把当前目录的文件引用(文件名)加到路径中
        p[DIRSIZ] = 0;

        if(stat(buf, &st) < 0){
            printf("ls: cannot stat %s\n", buf);
            continue;
        }
        find(buf, name);
    }
  }
  close(fd);
}

void main(int argc, char *argv[]){

    if(argc < 3){
        printf("need dir or pattern");
        exit(0);
    }
    else{
        find(argv[1],argv[2]);
    }
    exit(0);
}