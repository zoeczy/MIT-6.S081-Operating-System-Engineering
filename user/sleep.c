#include "kernel/types.h"
#include "user/user.h"

// int
// call_sleep(int num_time){
//     uint64 state = 0;
//     for(int i=0;i<num_time;i++){
//         state = sys_sleep();  // when return -1???
//         if(state == -1){
//             return -1;
//         }
//     }
//     return 0;
// }


int
main(int argc, char *argv[])
{
  if(argc == 2){
    //call_sleep(atoi(argv[0]));
    int state = sleep(atoi(argv[1]));  // use the api in user/usr.h not kernel/def.h
    if(state == 0){exit(0);}
    else{exit(-1);}
  }
  else if(argc == 1){
      printf("need a num");
      exit(1);
  }
  else{
      printf("how to sleep?");
      exit(1);
  }
}

