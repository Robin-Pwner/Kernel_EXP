This challenge is from QWB 2018



## Writeup

### 题目分析

给了一个叫core的模块，先用ida逆向一下

有两个全局变量一个是off还有一个全局数组name

总共有四个功能

write可以写name数组

ioctl有三个选项，从设置off,从栈上读，往栈上写

这个题考察的是kernel的栈溢出然后ROP

我们可以通过设置off，从栈上泄露canary。比较隐蔽的是找溢出点。

程序检查了往栈上写的size不能大于0x3f,正常是没办法覆盖return address的。

漏洞点在int向unsigned int 的转换上，做检查时用的是int,但memcpy时会把int转成unsigned int 16,因此我们可以输一个负数，就可以通过检查。

题目虽然开了kaslr,但是我们可以读/tmp目录下的kallsysms得到基地址。因为题目没开smap和smep，所以直接跳到用户态执行就行了。

### 解题思路

1. 设置off,然后从栈上读，泄露canary
2. 读文件获得kernel基地址
3. 利用write,把rop写到name
4. 输入一个负的size，执行从name向栈上拷贝，触发栈溢出，覆盖return address
5. 函数返回，跳到用户态执行，完成提权。
