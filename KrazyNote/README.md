This challenge is from Balsn CTF 2019

[KrazyNote](https://ctftime.org/task/9373)



## Writeup

### 题目分析

给了一个叫note的模块，先用ida逆向一下

发现总共有Add note,Show note,Edit Note,Reset 四个功能，

其中Note的结构体如下:

```c
------------------
|    KEY         |
------------------
|    Size        |
------------------
|    Offset = Content_addr - Page_offset_base
------------------
|    Content^KEY |
------------------
```

值得注意的有两点，一是Note中存的不是指向Content的指针，而是Content的地址减掉Page_offset_base，而是Content的内容会用KEY进行异或加密。

题目逻辑中的size都很正常，没有问题.题目的漏洞在于没有上锁，可能存在条件竞争。

正常情况下，因为进程会正常执行完一段逻辑才会进行进程切换，是不会出现条件竞争的。所以我们需要利用Copy_from_User和userfaultfd来强制触发进程切换，从而触发条件竞争，在另一个进程里先用Reset功能，然后重新分配Note,从而制造overlap.

有了overlap我们就可以修改Offset来进行任意读写了。

下面我们需要泄露cred结构体的地址。可以修改Offset　从Page_offset_base开始扫描内存，通过定位thread name,找到task struct, comm　上面就是指向cred的指针。

但要使Offset = cred_addr-Page_offset_base我们还需要泄露 Page_offset_base。由于Bss段存有指向Note结构体的指针，Note本身也存在Bss段上，所以可以先泄露Note_addr,和Offset,然后减去偏移得到Content_addr,这样就有Page_offset_base = Content_addr-Offset

最后我们修改Offset，使Content指向Cred,再用EDIT修改　Cred完成提权。

### 解题思路

1. 利用User fault fd制造Race condition从而制造　Note Overlap

2. 泄露KEY
3. 泄露Offset
4. 修改Offset泄露Content_addr 从而计算出Page offset base
5. 修改Offset从Page offset base起扫描内存，定位task　struct,泄露cred_addr
6. 修改Offset指向cred
7. 修改　cred struct 以提权
8. 执行execve("/bin/sh")