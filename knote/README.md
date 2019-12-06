This challenge is from D^3CTF 2019

[KNote](https://www.anquanke.com/post/id/193939#h3-10)



## Writeup

### 题目分析

给了一个叫knote的模块，先用ida逆向一下

有增删改查四个功能，其中增和删是有锁保护的，而改和查功能没有，因此存在Race condition,可以通过userfaultfd来稳定触发race condition。

先分配一个note，然后在get功能中触发userfaultfd，此时用另一个线程删除这个note，再分配一个tty结构体，覆盖原来note所在的位置，这样get到的就是tty结构体的内容，从而就有泄露。

这里的一个小点是，无法泄露tty结构体的前0x20字节，这是因为调用copy_user_generic_unrolled时，会先拷贝头0x20字节到寄存器，然后从寄存器拷贝到dst时才会触发userfaultfd,因此前0x20字节是之前的内容，而不是tty结构体的内容。这使得我们无法通过ops泄露kernel地址，需要用偏移0x250处的指针泄露Kernel地址。

有了地址之后，我们可以用类似的方法通过edit功能，修改tty结构体中的指针，修改ops来做jop/cop，但是我没找到什么合适的gadget，ropper给出的gadget大部分都是以ret结尾的。

所以这里采用的另一个办法，[修改modprobe path来越权执行](https://xz.aliyun.com/t/6067)

modprobe path是kernel的全局变量，我们通过条件竞争，修改一个空闲的slot的fd，使其指向modprobepath，然后分配两次，就可以修改modprobe path指向我们自己的脚本。这样当发生错误的时候就会以`root`权限去运行我们文件，读到flag。

这里还有两个我没搞明白的问题：

1.为什么最终的exp是有概率成功的，而不是稳定成功的。

2.现在的exp只是读flag文件，怎么用这种办法拿到root shell



### 解题思路

1. 利用User fault fd制造Race condition从而制造泄露
2. 利用User fault fd制造Race condition从而修改fd到mod probe path
3. 修改mod probe path到我们自己的文件
4. 编写一个简单的脚本
5. 触发 __request_module，执行我们的脚本，获得flag
