This challenge is from WCTF 2018

klist

Ref:http://p4nda.top/2018/11/27/wctf-2018-klist/

## Writeup

### 题目分析

本题涉及的堆块数据结构如下：

```c

00000000 struct_item     struc ; (sizeof=0x20)
00000000 refcount        dd ?
00000004 pad             dd ?
00000008 size            dq ?
00000010 fd              dq ?                 
00000018 buf             dq ?
00000020 struct_item     ends


```

堆块有0x18大小的header，buf及其后续为存储的具体内容，大小由用户控制。

对refcount使用原子操作加减，减为0时调用kfree释放结构体。用一个单向链表来存储所有的数据结构，fd指向下一个块。

ioctl里提供了四个功能，增和删是正常功能，select会将指向index个块的指针放到一个全局变量里，通过read和write可以读写这个全局变量指向的堆块的buf，但没有办法读写header。最后有个list head的功能，会将第一块内存中的数据返回给用户。

add 会调用get，remove会调用put，select和list head 会调用get和put，所有操作都有锁保护。

漏洞在于list head功能中锁加的不对，代码如下

```c
unsigned __int64 __fastcall list_head(__int64 a1)
{
  struct_item *v1; // rbx
  unsigned __int64 v2; // rbx

  mutex_lock(&list_lock);
  get(&g_list->isuse);
  v1 = g_list;
  mutex_unlock(&list_lock);
  v2 = -(signed __int64)((unsigned __int64)copy_to_user(a1, v1, v1->size + 0x18) >= 1) & 0xFFFFFFFFFFFFFFEALL;
  put(&g_list->isuse);
  return v2;
}
```

put操作在锁的外部，也就是说存在条件竞争，get和put的glist可以指向不同的对象。比如我们先插入一个obj_A，然后抵用list head,然后用另一个进程插入另一个对象obj_B，由于copy_to_user可能引起进程调度，使得put发生之前，可能obj_B已经插入成功，此时的glist指向的是obj_B而不是obj_A,obj_B此时的refcount为1，对其调用put会kfree obj_B,但glist依旧指向obj_B的地址，即我们获得了一个野指针，后续可以通过这个野指针进行第一想法还是打tty结构体。但这里有个问题，用tty结构体覆盖obj_B之后，对应size的地方正好是0，导致我们无法读写tty结构体除了头0x18字节以外的内容，所以不行。

panda的exp用的是do_msgsnd

我这里用的是pipe_buffer这个结构体，

这个结构体如下

```c
struct pipe_buffer{
	struct page* page;
	unsigned int offset,len;
	const struct pipe_buf_operations *ops;
	unsigned int flags;
	unsigned long private;
}
```

这个结构体的大小是40。

当我们调用pipe创建管道时，内核会新建一个pipe_inode_info对象，然后在alloc_pipe_info这个函数中

```c
pipe->bufs = kalloc(pipe_bufs,sizeof(struct pipe_buffer),GFP_KERNEL_ACCOUNT)
```

其中GFP_KERNEL_ACCOUNT为16，即一次会创建16个pipe_buffer对象，底层是通过kmalloc(0x280)实现的。

之后通过管道读写的内容都会通过这个pipe_buffer来实现。

当我们用这16个结构体去覆盖Obj_B时，对应size的是offset，我们可以通过管道读写把offset设置为0x1000(不知道能不能设的更大，我尝试读0x4000，发现变成了4个buffer每个的offset都是0x1000)，然后就可以越界读写后面slot上的内容了。

到这里方法就很多了，第一可以在后面spray tty的结构体，然后用tty的结构体打。

第二可以在后面spray 这个题的堆块结构，然后改fd，通过fd可以做到kernel内的任意读写，然后改cred结构体。

第三也可以打pipe_buffer里的pipe_buf_operation



这个题没开smap，所以是可以直接换栈ROP的，但是这个题开了KPTI，所以还需要绕过KPTI保护，正常来说可以通过signal(11,&rootshell)的方式来绕，但是这个题的signal不正常，没成功捕获到异常。所以改用通过gadget来绕过保护，利用swapgs_restore_regs_and_return_to_usermode中的mov    rdi,cr3。具体操作是跳到swapgs_restore_regs_and_return_to_usermode开头的	mov    rdi,rsp指令上，并且让此时的栈布置成

```c
	[rsp]   = 0;
	[rsp+1] = 0xdeadbeef;
	[rsp+2] = (uint64_t)&sudo;
	[rsp+3] = user_cs;
	[rsp+4] = user_rflags;
	[rsp+5] = user_sp;
	[rsp+6] = user_ss;

```

即可。



### 解题思路

1. 利用条件竞争制造野指针
2. 调用pipe，使野指针指向pipe_buffer
3. 对pipe获得fd调用write和read设置offset为0x1000
4. spray tty结构体，使其分配到pipe_buffer后面的0x1000以内的位置
5. 越界读tty结构体，泄露地址
6. 越界写tty的ops，指向伪造的vtable，vtable上放换栈的gadgets
7. 在伪造的栈上布置rop，包括提权，绕kpti，拿shell
