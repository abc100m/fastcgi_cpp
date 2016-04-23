#Welcome to use fastcgi_cpp
by: dragon jiang <  jianlinlong@gmail.com  >

fastcgi_cpp is a wrapper class for the fastcgi official C++ library package, it's simple and easy to use and powerful. 
Combined with nginx + fastcgi_cpp can quickly develop a highly efficient and flexible web application.


Features：
* Support request/response/session/cookie/file upload/...

* Easy interface

    for example, *Http_Request::get_parameter(..)* support both GET and POST
    
* Register handle by class name

    *Http_Application::add_mapping(const std::string& uri, const std::string& handle_class_name)*
    
* High performance, every line of code is the most efficient
    + *percent_decode()* for url decode is the most high performnace implement
    + use a *std::unordered_map* and a *std::list* to maintenance HttpSession which make session timeout expired in O(1) 
    + uploaded files were written into temporary file


here is a turial written in Chinese


###0.体系架构
欲知fastcgi程序的体系统架构，请访问这篇文章(此文画的图实在是太好了！)
[点我访问](http://www.cnblogs.com/skynet/p/4173450.html)

###1.安装nginx
[nginx官网下载地址](http://nginx.org/en/download.html)
下载稳定版，而后编译安装。参考如下步骤
```shell
#./configure
#make
#make install
```

###2.安装spawn-fcgi
[下载地址](https://github.com/lighttpd/spawn-fcgi)
spawn-fcgi是用来启动fastcgi程序的，我们开发好的可执行程序需要用这个工具来启动。 下载后请编译并安装

###3.安装fastcgi官方库
[下载地址](http://www.fastcgi.com/drupal/node/5)
下载后编译并安装
```shell
#./configure
#make
#make install
```
编译好的动态库位于 libfcgi/.libs ，并且已复制到 /usr/local/lib/目录下。如果某些情况程序找不到动态库，则需要配置LD_LIBRARY_PATH指向此目录。

###4.使用fastcgi_cpp
fastcgi_cpp提供了Http_Application、Http_Request、Http_Response、 Http_Session等几个类，业务开发只需从Http_Handle_Base继承实现handle并注册即可。我们来看一个Hello World:
```cpp
//hello.cpp
#include "fastcgi_cpp.h"
using namespace fastcgi_cpp;

class Hello_World : public Http_Handle_Base 
{
public:
    virtual void on_request(Http_Request& req, Http_Response& rsp)
    {
        rsp.set_header_content_type("text/html");
        rsp.write_data("hello world<br>");
        rsp.write_data("fastcgi_cpp is a modern c++ web framework base on fastcgi<br>");
    }
};
REGISTRAR_HANDLE_CLASS(Hello_World);//注册已经实现的Handler类名

int main(int argc, char* argv[])
{
    Http_Application app;
    app.add_mapping("hello_world.cgi", "Hello_World");//url访问地址与Handler绑定，可实现从配置文件中加载
    app.run();
}
```
短短几行代码一个WEB程序就写好了。
fastcgi_cpp只有头文件，但编译时需要链接到fastcgi官方库的动态库，Makefile请参考如下：
```shell
g++ -o hello hello.cpp -std=c++11 -lfcgi 
```

###5.程序部署
到此，我们已安装好了nginx和spawn-fcgi, 并且已经有一个hello可执行程序。

 - -->首先配置nginx的配置文件, 启动nginx:


```shell
[root@localhost /]# which nginx
/usr/local/nginx/sbin/nginx
[root@localhost /]# cd /usr/local/nginx/conf
```
编辑 nginx.conf 这个文件, 加入
```txt
location ~ \.cgi$ {
    root           html;
    fastcgi_pass   127.0.0.1:9002;
    fastcgi_index  index.cgi;
    fastcgi_param  SCRIPT_FILENAME  html$fastcgi_script_name;
    include        fastcgi_params;
}
```
这样, nginx就会把收到的请求转发到 9002 端口。nginx.conf的完整版请参考附件中的nginx.conf。然后启动nginx
```shell
#nginx
```

 - -->用spawn-fcgi 启动应用程序 
```shell
 #spawn-fcgi -a 127.0.0.1 -p 9002 ./hello
```
 
 - -->使用浏览器测试程序
```shell
[root@localhost .libs]# curl 127.0.0.1/hello_world.cgi
hello world<br>fastcgi_cpp is a modern c++ web framework base on fastcgi<br>
```
OK，我们的程序已经正常工作了

###6.更多例子
请参考fastcgi_test.cpp，共有6个demo
