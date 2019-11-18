This challenge is from Star CTF 2019

[Hackme](https://github.com/sixstars/starctf2019/tree/master/pwn-hackme)



## Writeup

### 题目分析

总共有增删改查 四个功能，

其中Note的结构体如下:

```c
------------------
|   size         |
------------------
|   ptr          |
------------------

```

从单线程角度讲，没有漏洞，free后指针清空了，edit也检查了size。唯一比较奇怪的是free之后size 没有清零。

这一点结合多线程思考就可以发现漏洞，因为add功能是先kmalloc然后copy from user然后再修改size,我们可以利用user fault fd在copy from user时让该线程卡主，再利用另一个线程访问该note,则此时的size是错的，我们可以用这个线程去做越界读写。

这个题开了semp和smap，所以我们不能换栈到用户态，只能换栈到kernel里。这里比较麻烦的是要通过调试寻找合适的地址来部署fake stack,并寻找合适的换栈的gadget。以及ropper找的gadget都不能用的话，要把vmlinux拖到ida里人肉找。由于没有直接xchg r?x,rsp这样的gadget ,我们需要通过mov rsp,r?x，这样的gadget来换栈，在这题只有mov rsp,rax 这个gadget，但是此时rax的值并不是我们能控制的，所以我们还额外需要一个gadget来设置rax。通过调试发现，r15 是我们能控制的，所以找到类似这样的gadget `mov rax, qword ptr [r15 + xxx ]; ...; call qword ptr [rax + xxx ];` 这样既可以控住rax,又能使其跳到下一条gadget。这里要注意不能把栈换到tty结构体的头部，不然会报错。

### 解题思路

1. 主线程分配一个size很大的note0，并free它
2. 再用另一个线程，利用user fault fd，分配note0，使其一直阻塞在改写size之前，此时ptr指向新的区域，但size依旧是之前的
3. 用主线程去访问note0 就可以越界读写
4. 分配一个ptmx，此时ptmx结构体应该在note后面
5. 通过越界读扫描，定位ptmx具体位置
6. 泄露kernel base地址 和ptmx结构体地址
7. 修改tty 结构体的vtable指向我们布置的fake vtable，
8. 在kernel 中布置fake stack 和rop
9. 调用 ptmx的ioctal，劫持控制流提权。
