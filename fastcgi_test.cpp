#include <iostream>
#include "fastcgi_cpp.h"
using namespace fastcgi_cpp;

//demo 1  hello world
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
REGISTRAR_HANDLE_CLASS(Hello_World);

void demo1(Http_Application& app) {
    app.add_mapping("hello_world.cgi", "Hello_World");
    //test url:  http://192.168.137.8/hello_world.cgi
}

//demo 2, header and parameter
class Header_Test: public Http_Handle_Base
{
public:
    virtual void on_request(Http_Request& req, Http_Response& rsp)
    {
        //print all headers
        std::map<std::string, std::string> headers = req.all_headers();
        std::stringstream ss1;
        for (auto const &k: headers) {
            ss1 << k.first << ":" << k.second << "<br>";
        }
        rsp.write_data("<b>request headers:</b><br>");
        rsp.write_data(ss1.str());

        //print all parameters
        std::unordered_multimap<std::string, std::string> parameters = req.all_parameters();
        std::stringstream ss2;
        for (auto const &k: parameters) {
            ss2 << k.first << ":" << k.second << "<br>";
        }
        rsp.write_data("<br><b>request parameters, support both get and post:</b><br>");
        rsp.write_data(ss2.str());

        std::stringstream ss3;
        ss3 << "a=" << req.get_parameter("a") << "<br>\n";
        ss3 << "c=" << req.get_parameter("c") << "<br>\n";
        rsp.write_data(ss3.str());
    }
};
REGISTRAR_HANDLE_CLASS(Header_Test);

void demo2(Http_Application& app) {
    app.add_mapping("see_header.cgi", "Header_Test");
    //test url:  http://192.168.137.8/see_header.cgi?a=b&c=100&xgyairu=alkflqtioqut&d=yes
}

//demo 3, redirect
class Redirect_Test: public Http_Handle_Base 
{
public:
    virtual void on_request(Http_Request& req, Http_Response& rsp)
    {
        rsp.redirect("http://www.github.com");
    }
};
REGISTRAR_HANDLE_CLASS(Redirect_Test);

void demo3(Http_Application& app) {
    app.add_mapping("redirect.cgi", "Redirect_Test");
    //test url: http://192.168.137.8/redirect.cgi
}
  
//demo 4, session
class Session_Test: public Http_Handle_Base
{
public:
    virtual void on_request(Http_Request& req, Http_Response& rsp)
    {
        rsp.set_header_content_type("text/html"); //set content type
        Http_Session *session = this->fetch_session(req, rsp, false);
        if (!session) {
            session = this->fetch_session(req, rsp, true);
            assert(session != nullptr);
            rsp.write_data("make new session......<br>");  //note: rsp.write_data(...) must after fetch_session()
            session->set("test_key", "hello session");
        } else {
            std::stringstream ss;
            ss << "session exists...<br>" << "session key is : test_key<br>";
            ss << "session value is:" << session->get("test_key");
            rsp.write_data(ss.str());
        }
    }
};
REGISTRAR_HANDLE_CLASS(Session_Test);

void demo4(Http_Application& app) {
    app.add_mapping("session.cgi", "Session_Test");
    //test url: http://192.168.137.8/session.cgi       http://192.168.137.8/session.cgi  
}

//demo 5, file upload
class File_Upload_Test: public Http_Handle_Base
{
private: 
    long filesize(std::FILE* f) {
        int prev=std::ftell(f);
        std::fseek(f, 0L, SEEK_END);
        long sz=std::ftell(f);
        std::fseek(f, prev, SEEK_SET);
        return sz;
    }

public:
    virtual void on_request(Http_Request& req, Http_Response& rsp)
    {
        //all files
        auto files = req.all_upload_files();
        std::stringstream ss;
        ss << "got " << files.size() << " upload file<br>\n";
        for (auto const& k: files) {
            std::string name = k.first;
            std::FILE* f     = k.second.first;      //req.get_upload_FILE(name);
            std::string f_name = k.second.second;   //req.get_upload_filename(name);
            ss << "name:" << name << ", file name:" << f_name << ", file size:" << filesize(f);
            ss << "<br>\n";
        }
        rsp.set_header_content_type("text/html");
        rsp.write_data(ss.str());

        //print all parameters
        std::unordered_multimap<std::string, std::string> parameters = req.all_parameters();
        std::stringstream ss2;
        for (auto const &k : parameters) {
            ss2 << k.first << ":" << k.second << "<br>";
        }
        rsp.write_data("<br><b>request parameters: </b><br>");
        rsp.write_data(ss2.str());
    }
};
REGISTRAR_HANDLE_CLASS(File_Upload_Test);

void demo5(Http_Application& app) {
    app.add_mapping("upload_file.cgi", "File_Upload_Test");
    //test --> file_upload.html
}


//demo6, with gzip
#include "gzip.h"
class Gzip_Test : public Http_Handle_Base
{
public:
    virtual void on_request(Http_Request& req, Http_Response& rsp)
    {
        const char xxx[] = "IIIIIIIIIIIIIIIIIIIIIIIIIII goooooOOOOOOOOOOOOOOt  moOOOOOOOOOOOOOOOOoooooooooony !!";
        std::string s;

        //compress
        bool success = gzipcodec::compress(xxx, sizeof(xxx) - 1, s);
        if (success) {
            rsp.set_header_content_type("text/plain");
            rsp.set_header_content_encoding("gzip");
            rsp.write_data((const char*)s.data(), s.length(), false);
        } else {
            rsp.write_data("something wrong...");
        }
    }
};
REGISTRAR_HANDLE_CLASS(Gzip_Test);

void demo6(Http_Application& app) {
    app.add_mapping("gzip.cgi", "Gzip_Test");
    //test --> http://192.168.137.8/gzip.cgi
}


int main(int argc, char *argv[])
{
    Http_Application app;
    demo1(app);
    demo2(app);
    demo3(app);
    demo4(app);
    demo5(app);
    demo6(app);

    app.run();

#if 0
    //using namespace std;
    //std::map<int, int> m;

    //m[1] = 100;
    //m[2] = 200;
    //m[3] = 300;
    //m[4] = 400;

    //auto it1 = m.find(1);
    //auto it2 = m.find(2);
    //auto it3 = m.find(3);
    //auto it4 = m.find(4);

    //m.erase(it2++);

    //cout << it1->second << "\t" << it3->second << "\t" << it4->second << endl;


    //struct X {
    //};

    //struct A: public X{
    //    int a = 100;
    //};
    //struct B: public X{
    //    int b = 999;
    //};


    //REGISTRAR_CLASS_2(A, X);
    //REGISTRAR_CLASS_2(B, X);
    //auto x1 = Instance_Factory<X>::instance()->create("A");
    //auto x2 = Instance_Factory<X>::instance()->create("B");

    ////Base *b = BaseFactory<void>::createInstance("DerivedB");
    ////auto b = Instance_Factory<Base>::instance()->create("Base");

    //Http_Response a;
    //Http_Request rx;

    //while (FCGI_Accept() >= 0) {

    //    //std::string s(NULL);

    //    FCGI_printf("Content-type: text/html;charset=utf8\r\n"
    //        "\r\n");

    //    FCGI_printf("hello---->\n");

    //    Http_Request r;

    //    FCGI_printf("<PRE>");
    //    std::map<std::string, std::string> vec = r.all_headers();
    //    for (auto const& s : vec){
    //        //cout << s << endl;
    //        FCGI_printf("%s=%s<br>", s.first.c_str(), s.second.c_str());
    //    }
    //    FCGI_printf("</PRE>\n");

    //    FCGI_printf("%lu\n", r.content_length());

    //    FCGI_printf("<PRE>");
    //    for (const auto& p : r.all_parameters()) {
    //        FCGI_printf("%s:%s\n", p.first.c_str(), p.second.c_str());
    //    }
    //    FCGI_printf("</PRE>\n");
    //}

    ////
    //srand(time(NULL));

    //for (int i=0; i<50; ++i) {

    //    cout << UUID_V4::uuid() << endl;
    //}

    //return 0;
#endif
}

