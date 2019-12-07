This challenge is from 0ctf 2018 

Baby Final

Ref: http://p4nda.top/2018/07/20/0ctf-baby/



## Writeup

### 题目分析

flag就在模块里，题目只有两个功能。

第一个功能是用printk打印flag的地址。需要用dmesg来获取printk输出的内容

第二个功能是输入一个flag然后和内置的flag的比较，比较之前有一段检查，我们称指向我们输入的flag的指针是ptr，该检查要求ptr的值小于  0x7ffffffff000，即是用户态的地址，不能让ptr直接指向内容中的flag。如果通过比较，就会打印flag的内容。

这是一个典型的double fetch的问题，在做check时第一次用到了ptr，是第一次fetch，在做内容比较时是第二次fetch，两次fetch外没有加锁，使得可以在check之后改变ptr的值。

即一开始让ptr指向用户态，这样可以通过第一个check，然后利用条件竞争，在比较之前改写ptr，使其指向内核中真正的flag，这样即可通过比较检查。



此外这个题还可以通过侧信道逐字节爆破flag。大概思路是让当前爆破的字节的下一个字节放在一个不可读的段上，那么如果当前爆破的字节正确，就会触发panic，否则就是不正确。通过这个信息可以确定当前字节是否爆破成功。



### 解题思路

1. 获得内核中的flag地址
2. 让ptr指向用户态，调用模块，进行比较，
3. 与此同时，用另一个线程去不停修改ptr为内核中真实flag的地址
4. 重复上述操作0x1000次

