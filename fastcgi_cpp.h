/**
* Copyright(c) 2016 dragon jiang<jianlinlong@gmail.com>
* LGPL license
*/

#ifndef _FASTCGI_CPP11_
#define _FASTCGI_CPP11_

#define FASTCGI_CPP_BEGIN namespace fastcgi_cpp {
#define FASTCGI_CPP_NED   }

#define NO_FCGI_DEFINES
#include <fcgi_stdio.h>    //另一种方案：  http://althenia.net/fcgicc  方便调试
#include <cstdio>       //tmpfile()
#include <string>
#include <cstring>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <list>
#include <sstream>
#include <assert.h>
#include <random>
#include "dynamic_factory.h"

//引用外部的全部环境变量  
#ifdef _WIN32
#include <process.h>
#else
extern char **environ;
#endif


FASTCGI_CPP_BEGIN

namespace detail {
    inline bool startswith(const char* d1, size_t d1_len, const char * d2, size_t d2_len) {
        return d1_len >= d2_len && (::memcmp(d1, d2, d2_len) == 0);
    }

    inline bool endswith(const char* d1, size_t d1_len, const char * d2, size_t d2_len) {
        return d1_len >= d2_len && (::memcmp(d1 + d1_len - d2_len, d2, d2_len) == 0);
    }

    inline bool strequ(const char* d1, size_t d1_len, const char * d2, size_t d2_len) {
        return d1_len == d2_len && (::memcmp(d1, d2, d2_len) == 0);
    }

    //copy from fgets but changed return value
    int our_fgets(char *dst, int max, FCGI_FILE *fp){
        int c;
        char *p;

        for (p = dst, max--; max > 0; max--) {
            if ((c = FCGI_fgetc(fp)) == EOF)
                break;
            *p++ = c;
            if (c == '\n')
                break;
        }
        *p = 0;
        if (p == dst || c == EOF)
            return -1;
        return p - dst;
    }
};

/*
* uri decode
*from：http://stackoverflow.com/questions/2673207/c-c-url-decode-library
*reference：http://bogomip.net/blog/cpp-url-encoding-and-decoding/     http://codepad.org/lCypTglt
*           https://github.com/eidheim/Simple-Web-Server/issues/11  nice!
*/
template<typename Iterator>
bool percent_decode(std::string& out, Iterator it, Iterator end)
{
    static const char tbl[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
         0, 1, 2, 3, 4, 5, 6, 7,  8, 9,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,10,11,12,13,14,15,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1
    };
    out.clear();
    out.reserve(std::distance(it, end));
    char c, a, b;
    while(it!=end) {
        c = *it++;
        if (c=='%') {
            if(it==end || (a=tbl[static_cast<unsigned char>(*it++)])<0 ||
               it==end || (b=tbl[static_cast<unsigned char>(*it++)])<0) {
                out.clear();
                return false;
            }
            c=(a<<4)|b;
        }
        out.push_back(c);
    }
    return true;
}

//generate unique id. UUID version 4 which specified in RFC 4122, see: https://en.wikipedia.org/wiki/Universally_unique_identifier#Implementations
class UUID_V4 
{
public:
    static std::string uuid() {
        std::random_device rd;

        char buf[64];
        sprintf(buf, "%x%x-%x-%x-%x-%x%x%x", 
                rd(), rd(),                   // Generates a 64-bit Hex number
                rd(),                         // Generates a 32-bit Hex number
                ((rd() & 0x0fff) | 0x4000),   // Generates a 32-bit Hex number of the form 4xxx (4 indicates the UUID version)
                (rd() & 0x3fff) + 0x8000,     // Generates a 32-bit Hex number in the range [0x8000, 0xbfff]
                rd(), rd(), rd()              // Generates a 96-bit Hex number
                );        
        return buf;
    }
};


//http cookie. reference:: HTTPCookie of cgicc library. 
class HTTP_Cookie
{
public:
    HTTP_Cookie()
        : maxage_(0),
        secure_(false),
        removed_(false)
    {}

    HTTP_Cookie(const std::string& name, const std::string& value)
        : name_(name),
        value_(value),
        maxage_(0),
        secure_(false),
        removed_(false)
    {}

    HTTP_Cookie(const std::string& name,
                const std::string& value,
                unsigned long maxAge,
                const std::string& path,
                const std::string& comment,
                const std::string& domain,                
                bool secure)
        : name_(name),
        value_(value),
        maxage_(maxAge),
        path_(path),
        comment_(comment),
        domain_(domain),
        secure_(secure),
        removed_(false)
    {}

    HTTP_Cookie(const HTTP_Cookie& cookie)
        :name_(cookie.name_),
        value_(cookie.value_),
        comment_(cookie.comment_),
        domain_(cookie.domain_),
        maxage_(cookie.maxage_),
        path_(cookie.path_),
        secure_(cookie.secure_),
        removed_(cookie.removed_)
    {}

    inline bool operator != (const HTTP_Cookie& cookie) const
    {
        return !operator==(cookie);
    }

#ifdef WIN32
    /* Dummy operator for MSVC++ */
    inline bool operator< (const HTTP_Cookie& cookie) const {
        return false;
    }
#endif

    inline void remove() {
        removed_ = true;
    }

    inline void set_removed(bool removed) {
        removed_ = removed;
    }

    inline bool is_removed() const {
        return removed_;
    }

    inline std::string get_name() const {
        return name_;
    }

    inline void set_name(const std::string& name) {
        name_ = name;
    }

    inline std::string get_value() const {
        return value_;
    }

    inline void set_value(const std::string& value) {
        value_ = value;
    }

    inline void set_comment(const std::string& comment) {
        comment_ = comment;
    }

    inline std::string get_comment() const {
        return comment_;
    }

    inline void set_domain(const std::string& domain) {
        domain_ = domain;
    }

    inline std::string get_domain() const {
        return domain_;
    }

    inline void set_maxage(unsigned long maxAge) {
        maxage_ = maxAge;
    }

    inline unsigned long get_maxage() const {
        return maxage_;
    }

    inline void set_path(const std::string& path) {
        path_ = path;
    }

    inline std::string get_path() const {
        return path_;
    }

    inline void set_secure(bool secure) {
        secure_ = secure;
    }

    inline bool is_secure() const {
        return secure_;
    }

    std::string to_string() const {
        std::stringstream out;
        out /*<< "Set-Cookie:"*/ << name_ << '=' << value_;
        if (false == comment_.empty())
            out << "; Comment=" << comment_;
        if (false == domain_.empty())
            out << "; Domain=" << domain_;
        if (removed_)
            out << "; Expires=Fri, 01-Jan-1971 01:00:00 GMT;";
        else if (0 != maxage_)
            out << "; Max-Age=" << maxage_;
        if (false == path_.empty())
            out << "; Path=" << path_;
        if (true == secure_)
            out << "; Secure";

        out << "; Version=1";
        return out.str();
    }

    bool operator== (const HTTP_Cookie& cookie) const {
        return (name_ == cookie.name_
            && value_ == cookie.value_
            && comment_ == cookie.comment_
            && domain_ == cookie.domain_
            && maxage_ == cookie.maxage_
            && path_ == cookie.path_
            && secure_ == cookie.secure_
            && removed_ == cookie.removed_);
    }

    //from cgicc
    static int parse_cookie(const std::string& data, HTTP_Cookie& cookie) {
        // find the '=' separating the name and value
        std::string::size_type pos = data.find("=", 0);
        if (std::string::npos == pos)
            return -1;

        // skip leading whitespace - " \f\n\r\t\v"
        std::string::size_type wscount = 0;
        std::string::const_iterator data_iter;
        for (data_iter = data.begin(); data_iter != data.end(); ++data_iter, ++wscount)
            if (0 == std::isspace(*data_iter))
                break;

        // Per RFC 2091, do not unescape the data (thanks to afm@othello.ch)
        std::string name = data.substr(wscount, pos - wscount);
        std::string value = data.substr(++pos);

        cookie.set_name(name);
        cookie.set_value(value);
        return 0;
    }

    //from cgicc,  works for request cookie only
    static std::vector<HTTP_Cookie> parse_cookies(const std::string& data) {
        std::vector<HTTP_Cookie> vec;
        if (!data.empty()) {
            std::string::size_type pos;
            std::string::size_type oldPos = 0;
            int n;

            while (true) {
                // find the ';' terminating a name=value pair
                pos = data.find(";", oldPos);

                HTTP_Cookie cookie;
                // if no ';' was found, the rest of the string is a single cookie
                if (std::string::npos == pos) {
                    n = parse_cookie(data.substr(oldPos), cookie);
                    if (-1 != n) {
                        vec.emplace_back(cookie);
                    }
                    break;
                }

                // otherwise, the string contains multiple cookies
                // extract it and add the cookie to the list
                n = parse_cookie(data.substr(oldPos, pos - oldPos), cookie);
                if (-1 != n) {
                    vec.emplace_back(cookie);
                }

                // update pos (+1 to skip ';')
                oldPos = pos + 1;
            }
        }
        return vec;
    }

private:
    std::string 	name_;
    std::string 	value_;
    std::string 	comment_;
    std::string 	domain_;
    unsigned long 	maxage_;
    std::string 	path_;
    bool 		    secure_;
    bool		    removed_;
};


// http://www.stefanfrings.de/qtwebapp/api/classHttpSession.html
class Http_Session
{
public:
    Http_Session() {
        id_ = UUID_V4::uuid();
        last_access_ = time(NULL);
    }

public:

    const std::string& session_id() const {
        return id_;
    }

    bool has_key(const std::string& key) {
        return data_.find(key) != data_.end();
    }

    std::string get(const std::string& key) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second;
        }
        return std::string();
    } 

    void set(const std::string& key, const std::string& val) {
        data_[key] = val;
    }

    void remove(const std::string& key) {
        auto it = data_.find(key);
        if (it != data_.end()) {
            data_.erase(it);
        }
    }

    time_t access_time() const {
        return last_access_;
    }

    void set_access_time(time_t t) {
        last_access_ = t;
    }

private:
    std::string id_;
    std::unordered_map<std::string, std::string> data_;
    time_t last_access_;
};


//没有处理字符集(将cpp文件的字符集设为utf8, 默认为utf-8)
class Http_Request
{
public:
    Http_Request()
        : parameter_built_(false)
        , cookies_built_(false)
        , upload_file_built_(false)
    {}

    ~Http_Request() {
        //close all uploaded temporary file
        for (auto &k : upload_files_) {
            std::fclose(k.second.first);
        }
    }

protected:
    /**
    * get environment value
    */
    const char* getenv_data(const char* name) const {
        return ::getenv(name);
    }

    std::string getenv_string(const char* name) const {
        char *p = ::getenv(name);
        return p ? p : std::string();
    }

public:
    /**
    * http://nginx.org/en/docs/http/ngx_http_fastcgi_module.html#Parameters.2C_transferred_to_FastCGI-server
    * HTTP request header fields are passed to a FastCGI server as parameters. 
    * In applications and scripts running as FastCGI servers, these parameters are usually made available as environment variables. 
    * For example, the “User-Agent” header field is passed as the HTTP_USER_AGENT parameter. In addition to HTTP request header fields, 
    * it is possible to pass arbitrary parameters using the fastcgi_param directive.
    */
    std::string get_http_header(const char* header) {
        std::string h("HTTP_");
        h.append(header);
        return getenv_string(h.c_str());
    }
    
    std::string get_http_header(const std::string& header) {
        return get_http_header(header.c_str());
    }

    std::string get_raw_header(const char* header) {
        return getenv_string(header);
    }
    
    /*
    * CGI脚本的的名称, 全路径
    */
    std::string scripte_file_name() const {
        return getenv_string("SCRIPT_FILENAME");
    }
    
    /* CGI脚本的的名称 */
    std::string script_name() const {
        return getenv_string("SCRIPT_NAME");
    }
        
    std::string user_agent() const {
        return getenv_string("HTTP_USER_AGENT");
    }
    
    /* request mothod: GET, POST */
    std::string request_method() const {
        return getenv_string("REQUEST_METHOD");
    }
    
    /* content type */
    std::string content_type() const {
        return getenv_string("CONTENT_TYPE");
    }
    
    /* content length */
    size_t content_length() const {
        const char *data = getenv_data("CONTENT_LENGTH");
        if (!data) {
            return 0;
        }
        char *end;
        return ::strtoul(data, &end, 10);
    }
    
    /*  request uri  . 
    * 这个变量等于包含一些客户端请求参数的原始URI，它无法修改，请查看$uri更改或重写URI，不包含主机名，例如：”/cnphp/test.php?arg=freemouse”。
    * e.g.:  /cnphp/test.php?arg=freemouse 
    */
    std::string request_uri() const {
        return getenv_string("REQUEST_URI");
    }
    
    /*
    * 请求中的当前URI(不带请求参数，参数位于$args)，可以不同于浏览器传递的$request_uri的值，它可以通过内部重定向，或者使用index指令进行修改，
    * $uri不包含主机名，如”/foo/bar.html”。
    */
    std::string document_uri() const {
        return getenv_string("DOCUMENT_URI"); 
    }
    
    /* 当前请求的文档根目录或别名 */
    std::string document_root() const {
        return getenv_string("DOCUMENT_ROOT"); 
    }

    /* 服务器的HTTP版本, 通常为 “HTTP/1.0” 或 “HTTP/1.1” */
    std::string server_protocol() const {
        return getenv_string("SERVER_PROTOCOL");
    }
    
    /* "on" if SSL enabled otherwise null string */
    std::string https() const {
        return getenv_string("HTTPS");
    }
    
    /*  eg.: CGI/1.1 */
    std::string gateway_interface() const {
        return getenv_string("GATEWAY_INTERFACE");
    }
    
    /* eg.: nginx/1.8 */
    std::string server_software() const {
        return getenv_string("SERVER_SOFTWARE");
    }
    
    /* client ip address*/
    std::string remote_addr() const {
        return getenv_string("REMOTE_ADDR");
    }
    
    /* client port*/
    std::string remote_port() const {
        return getenv_string("REMOTE_PORT"); 
    }
    
    /*server ip address*/
    std::string server_addr() const {
        return getenv_string("SERVER_ADDR");
    }
    
    /* server port */
    std::string server_port() const {
        return getenv_string("SERVER_PORT");
    }
    
    /* server name */
    std::string server_name() const {
        return getenv_string("SERVER_NAME");
    }

public:

    //message body, fetch data --> std::string() -> data(), length()
    const std::string& get_body() {
        if (!body_) {
            std::string *s = new std::string();
            size_t len = content_length();
            if (len > 0) {
                s->resize(len);
                FCGI_fread((char*)s->data(), 1, len, FCGI_stdin);
            }
            else {
                char buf[8192];
                while ((len = FCGI_fread(buf, 1, sizeof(buf), FCGI_stdin)) > 0) {
                    s->append(buf, len);
                }
            }
            body_.reset(s);
        }
        return *body_;
    }

    std::map<std::string, std::string> all_headers() const {
        std::map<std::string, std::string> v;
        for (char** e = environ; *e; e++) {
            std::string s(*e);
            auto pos = s.find('=');
            if (pos != std::string::npos) {
                v.insert(std::make_pair(s.substr(0, pos), s.substr(pos + 1)));
            }
        }
        return v;
    }

    std::string get_parameter(const std::string& name) {
        if (!parameter_built_) {
            build_parameters();
            parameter_built_ = true;
        }
        auto it = parameters_.find(name);
        if (it != parameters_.end()) {
            return it->second;
        }
        return "";
    }

    std::vector<std::string> get_parameters(const std::string& name) {
        if (!parameter_built_) {
            build_parameters();
            parameter_built_ = true;
        }
        std::vector<std::string> vec;
        auto range = parameters_.equal_range(name);
        for (auto it = range.first; it != range.second; ++it) {
            vec.emplace_back(it->second);
        }
        return vec;
    }

    const std::unordered_multimap<std::string, std::string>& all_parameters() {
        if (!parameter_built_) {
            build_parameters();
            parameter_built_ = true;
        }
        return parameters_;
    }

    std::string get_cookie(const std::string& cookie) {
        if (!cookies_built_) {
            build_cookies();
            cookies_built_ = true;
        }
        auto it = cookies_.find(cookie);
        if (it != cookies_.end()) {
            return it->second;
        }
        return std::string();
    }

    const std::unordered_map<std::string, std::string>& all_cookies() {
        if (!cookies_built_) {
            build_cookies();
            cookies_built_ = true;
        }
        return cookies_;
    }

    std::FILE* get_upload_FILE(const std::string& name) {
        return get_upload_pair(name).first;
    }

    std::string get_upload_filename(const std::string& name) {
        return get_upload_pair(name).second;
    }

    const std::unordered_map<std::string, std::pair<std::FILE*, std::string> >& all_upload_files() {
        if (!upload_file_built_) {
            build_multipart();
            upload_file_built_ = true;
        }
        return upload_files_;
    }

protected:
    std::pair<std::FILE*, std::string> get_upload_pair(const std::string& name) {
        if (!upload_file_built_) {
            build_multipart();
            upload_file_built_ = true;
        }

        auto it = upload_files_.find(name);
        if (it != upload_files_.end()) {
            return it->second;
        }
        return std::make_pair(nullptr, "");
    }

    int build_multipart() {
        static const char mutilpart_form_data[] = "multipart/form-data; boundary=";
        static const char content_disposition_form_data[] = "Content-Disposition: form-data; name=";
        static const char CRLF[] = "\r\n";

        //boundary
        const char* p = getenv_data("CONTENT_TYPE");
        if (!detail::startswith(p, ::strlen(p), mutilpart_form_data, sizeof(mutilpart_form_data) - 1)) {
            return -1;
        }
        std::string boundary("--");
        boundary.append(p + sizeof(mutilpart_form_data) - 1);

        enum State { state_crlf, state_boundary, state_key_value, state_body_crlf, state_body, state_done };
        State state = state_boundary;

        const int buf_size = 8192;
        char data[2 + buf_size + 6], name[256], filename[256], *buf = data + 2;
        std::FILE* tmpf = NULL; //temporary  file
        int buf_len, boundary_len = boundary.length();
        bool line_end_with_crlf;

        data[0] = '\r'; data[1] = '\n';
        while ((buf_len = detail::our_fgets(buf, buf_size, FCGI_stdin)) != -1) {
            switch (state)
            {
            case state_crlf:
                if (::strcmp(buf, CRLF) == 0) {
                    state = state_boundary;
                }
                break;
            case state_boundary:
                state = state_key_value;
                break;
            case state_key_value:
                if ( detail::startswith(buf, buf_len, content_disposition_form_data, sizeof(content_disposition_form_data) - 1) ) {
                    if (tmpf) {
                        std::rewind(tmpf);
                        tmpf = NULL;
                    }

                    //format is -->"key1" or -->"file"; filename="d:\1.rar"
                    char *p = buf + sizeof(content_disposition_form_data) - 1;
                    name[0] = filename[0] = '\0';
                    if (::strstr(p, ";")) {
                        if ( ::sscanf(p, "%*[\"]%[^\"]%*[^=]%*[=\"]%[^\"\r\n]", name, filename) > 0 ) {  // %*  means skip
                            tmpf = std::tmpfile();
                            upload_files_[name] = std::make_pair(tmpf, filename);
                        }
                    } else {
                        if ( ::sscanf(p, "%*[\"]%[^\"]", name) > 0 ) {
                            parameters_.insert(std::make_pair(name, ""));
                        }
                    }
                    state = state_body_crlf;
                    if ( '\0' == name[0] ) {
                        return -2; //error
                    }
                }
                break;
            case state_body_crlf:
                if (::strcmp(buf, CRLF) == 0) {
                    state = state_body;
                    line_end_with_crlf = false;
                }
                break;
            case state_body:
                if ( detail::startswith(buf, buf_len, boundary.c_str(), boundary_len) ) {
                    if ( detail::strequ(buf + boundary_len, buf_len - boundary_len, CRLF, 2) ) {
                        state = state_key_value; break;
                    }
                    else if ( detail::strequ(buf + boundary_len, buf_len - boundary_len, "--\r\n", 4) ) {
                        state = state_done; break;// this is the end of boundary
                    }
                }

                {
                    int data_delta = 0, len_delta = 0;
                    if (line_end_with_crlf) {  //is prev line CRLF truncated ?
                        data_delta  = -2;
                        len_delta  += 2;
                    }
                    if (line_end_with_crlf = detail::endswith(buf, buf_len, CRLF, 2)) {
                        len_delta -= 2;  //remove CRLF
                    }

                    if (tmpf) {
                        std::fwrite(buf + data_delta, 1, buf_len + len_delta, tmpf);
                    } else {
                        auto it = parameters_.find(name);
                        assert(it != parameters_.end());  //must be: tmpf or parameter
                        it->second.append(buf + data_delta, buf_len + len_delta);
                    }
                }
                break;
            default:
                break;
            }
        } //end of while

        if (tmpf) {
            std::rewind(tmpf);
        }
        return 0;
    }

    void build_parameters() {
        const char* data = nullptr;
        std::string method = this->request_method();
        if (method == "GET") {
            data = getenv_data("QUERY_STRING");
        }
        else if (method == "POST") {
            static const char APP_FORMDATA[]   = "multipart/form-data";
            static const char APP_TEXT_PLAIN[] = "text/plain";
            const char* ctype = getenv_data("CONTENT_TYPE");
            int len = ::strlen(ctype);
            if ( detail::startswith(ctype, len, APP_FORMDATA, sizeof(APP_FORMDATA) - 1) ) {
                build_multipart();
                return;
            }
            if ( detail::startswith(ctype, len, APP_TEXT_PLAIN, sizeof(APP_TEXT_PLAIN) - 1) ) {
                return;
            }
            data = this->get_body().data();  //default: application/x-www-form-urlencoded
        }

        //parse key value
        if (data) {
            const char *p, *q, *c;
            p = q = c = data;

            while (*p) {
                if (*p == '=') {
                    c = p;
                }
                else if (*p == '&' || *(p+1) == '\0') {
                    std::string key, val;
                    percent_decode(key, q, c);
                    percent_decode(val, c+1, *(p+1) ? p : p+1);
                    parameters_.insert(std::make_pair(key, val));
                    q = c = (p + 1);
                }
                ++p;
            }
        }
    }

    void build_cookies() {
        std::string data = this->getenv_string("HTTP_COOKIE");
        std::vector<HTTP_Cookie> cookeis = HTTP_Cookie::parse_cookies(data);
        for (auto const& k : cookeis) {
            cookies_.insert(std::make_pair(k.get_name(), k.get_value()));
        }
    }

private:
    std::unique_ptr<std::string>                                         body_;
    std::unordered_multimap<std::string, std::string>                    parameters_;
    std::unordered_map<std::string, std::string>                         cookies_;
    std::unordered_map<std::string, std::pair<std::FILE*, std::string> > upload_files_;  //field_name, temp_file
    bool parameter_built_, cookies_built_, upload_file_built_;
    //接口参考： http://www.stefanfrings.de/qtwebapp/api/classHttpRequest.html
};


//接口参考： http://www.stefanfrings.de/qtwebapp/api/classHttpResponse.html
class Http_Response
{
public:
    Http_Response()
        :sent_headers_(false)
        , chunked_mode_(false)
        , sent_last_part_(false)
        , direct_chunk_(false)
    {
        set_status(200, "OK");
    }

    ~Http_Response() {
        if (chunked_mode_ && !sent_last_part_) {
            write_data(nullptr, 0, true);
        }
    }

    void set_direct_chunk(bool t) {
        direct_chunk_ = t;
    }

public:
    void set_status(int code, const std::string& msg) {
        status_code_ = code;
        status_message_ = msg;
    }

    void set_header(const std::string& header, const std::string& value) {
        headers_[header] = value;
    }

    void set_header_content_type(const std::string& stype) {
        set_header("Content-Type", stype);
    }

    void set_header_content_encoding(const std::string& encoding) {
        set_header("Content-Encoding", encoding);
    }

    void set_cookie(const HTTP_Cookie& cookie) {
        cookies_[cookie.get_name()] = cookie;
    }

    std::string get_cookie(const std::string& name) {
        auto it = cookies_.find(name);
        if (it != cookies_.end()) {
            return it->second.get_value();
        }
        return std::string();
    }

    /**
    * append data to http server
    * @last_part, must be true when send last part data
    */
    void write_data(const char* data, int data_len, bool last_part = false) {
        if (!direct_chunk_) {
            if (!sent_headers_) {
                write_header();
                sent_headers_ = true;
            }
            if (data && data_len > 0) {
                FCGI_fwrite((void*)data, 1, data_len, FCGI_stdout);
            }
            return;
        }

        //direct chunked. QtWebApp's way, from: http://www.stefanfrings.de/qtwebapp/api/httpresponse_8cpp_source.html
        if (!sent_headers_) {
            if (last_part) {
                set_header("Content-Length", std::to_string(data_len));
            } else {
                bool connection_close = false;
                auto it = headers_.find("Connection");
                if (it != headers_.end() && it->second == "close") {
                    connection_close = true;
                }
                if (!connection_close) {
                    set_header("Transfer-Encoding", "chunked");
                    //set_header("X-Accel-Buffering", "no"); //fastcgi_buffering off;
                    //header('Vary: Accept-Encoding');
                    chunked_mode_ = true;
                }
            }
            write_header();
            sent_headers_ = true;
        }

        // Send data
        if (data && data_len > 0) {
            if (chunked_mode_) {
                FCGI_printf("%lx\r\n", data_len);  
                FCGI_fwrite((void*)data, 1, data_len, FCGI_stdout);
                FCGI_printf("\r\n");
            } else {
                FCGI_fwrite((void*)data, 1, data_len, FCGI_stdout);
            }
        }

        // Only for the last chunk, send the terminating marker and flush the buffer.
        if (last_part) {
            if (chunked_mode_) {
                FCGI_printf("0\r\n\r\n");
            }
            sent_last_part_ = true;
        }
    }

    void write_data(const std::string& data, bool last_part = false) {
        write_data(data.c_str(), data.length(), last_part);
    }

    void write_data(const char* data, bool last_part = false) {
        write_data(data, ::strlen(data), last_part);
    }

    void redirect(const std::string& url) {
        set_status(303, "See Other");
        set_header("Location", url);
        write_data(std::string("Redirect"), true);
    }

private:
    void write_header() {
        std::stringstream ss;
        //ss << "HTTP/1.1 " << status_code_ << ' ' << status_message_ << "\r\n";
        ss << "Status: " << status_code_ << ' ' << status_message_ << "\r\n";
        for (auto const& h : headers_) {
            ss << h.first << ": " << h.second << "\r\n";
        }
        for (auto const& k : cookies_) {
            ss << "Set-Cookie: " << k.second.to_string() << "\r\n";
        }
        ss << "\r\n";

        //FCGI_printf(ss.str().c_str());
        std::string s = ss.str();
        FCGI_fwrite((void*)s.c_str(), 1, s.length(), FCGI_stdout);
    }

private:
    std::string status_message_;
    std::map<std::string, std::string> headers_;  //keep headers order
    std::map<std::string, HTTP_Cookie> cookies_;
    int  status_code_;
    bool sent_headers_;
    bool chunked_mode_;
    bool sent_last_part_;
    bool direct_chunk_;
};


class Http_Application;
class Http_Handle_Base
{
public:
    virtual ~Http_Handle_Base() {}

    virtual int init() {
        return 0; 
    }

    virtual void on_request(Http_Request& req, Http_Response& rsp)
    {

    }

    Http_Session* fetch_session(Http_Request& req, Http_Response& rsp, bool allow_create=true);

private:
    friend class Http_Application;
    Http_Application *app_;

    void set_owner_app(Http_Application* app) {
        app_ = app;
    }
};


#define REGISTRAR_HANDLE_CLASS(class_name) REGISTRAR_CLASS_2(class_name, Http_Handle_Base)


class Http_Application
{
public:
    Http_Application()
        :session_cookie_name_("FSESSION")
        ,session_alive_seconds_(60 * 60)    //60 minutes
        , session_check_interval_(15 * 60)  //15 minutes
        , session_checktime_(time(NULL))
    {
    }

    ~Http_Application() {
        clear_all_instance();
    }

    void set_session_cookie_name(const std::string& s) {
        session_cookie_name_ = s;
    }

    void set_cookie_domain(const std::string& s) {
        session_cookie_domain_ = s;
    }

    void set_cookie_path(const std::string& s) {
        session_cookie_path_ = s;
    }

    void set_cookie_comment(const std::string& s) {
        session_cookie_comment_ = s;
    }

    void set_session_alive_seconds(unsigned int n) {
        session_alive_seconds_ = n;
    }

    void set_session_check_interval(unsigned int n) {
        session_check_interval_ = n;
    }

public:

    /**
    * add uri worker class
    */
    void add_mapping(const std::string& uri, const std::string& handle_class_name) {
        uri_class_mapping_[uri] = handle_class_name;
    }

    void set_default_handler(const std::string& handle_class_name) {
        default_handle_class_ = handle_class_name;
    }

    void run() {
        while (FCGI_Accept() >= 0) {
            Http_Request req;
            Http_Response rsp;
            
            std::string class_name = find_uri_class(req.document_uri());
            if (class_name.empty()) {
                class_name = default_handle_class_; //default handle
            }

            if (!class_name.empty()) {
                auto instance = get_class_instance(class_name);
                if (instance) {
                    instance->on_request(req, rsp);  //do work
                }
            }

            //clear timeout session. do it in another thread? but that will need LOCK!
            clear_timeout_session();
        }
    }

    Http_Session* ensure_session_exists(Http_Request& req, Http_Response& rsp, bool allow_create=true) {
        Http_Session *session = nullptr;
        time_t now = time(NULL);

        auto val = req.get_cookie(session_cookie_name_);
        if (val.empty()) {
            val = rsp.get_cookie(session_cookie_name_);
        }

        if (!val.empty()) {
            auto it = session_map_.find(val);
            if (it != session_map_.end()) {
                auto instance = it->second.first;
                session_list_.erase(it->second.second);

                if (instance->access_time() + session_alive_seconds_ < now) {
                    //cookie timeout
                    session_map_.erase(it);
                } else {
                    //reorder session list by access time
                    it->second.second = session_list_.insert(session_list_.end(), instance);
                    session = instance.get();
                }
            }
        }

        //make new session
        if (!session && allow_create) {
            session = new Http_Session();
            std::shared_ptr<Http_Session> instance(session);
            auto list_it = session_list_.insert(session_list_.end(), instance);
            session_map_[session->session_id()] = std::make_pair(instance, list_it);
        }

        if (session) {
            HTTP_Cookie c(session_cookie_name_, session->session_id(), 
                          session_alive_seconds_, session_cookie_path_, 
                          session_cookie_comment_, session_cookie_domain_,
                          false
                         );  //to do: set other cookie fields
            rsp.set_cookie(c);
            session->set_access_time(now);
        }

        return session;
    }
   
protected:

    std::string mapping_class(const std::string& name) {
        if (!name.empty()) {
            auto it = uri_class_mapping_.find(name);
            if (it != uri_class_mapping_.end()) {
                return it->second;
            }
        }
        return std::string();
    }

    std::string find_uri_class(const std::string& uri) {
        //user setup mapping
        auto str1 = mapping_class(uri);
        if (!str1.empty()) {
            return str1;
        }

        //  /abc/do.cgi --> do.cgi
        std::string cgi_name;
        auto pos = uri.find_last_of('/');
        if (pos != std::string::npos && (pos + 1) < uri.length()) {
            cgi_name = uri.substr(pos + 1);
            auto str2 = mapping_class(cgi_name);
            if (!str2.empty()) {
                return str2;
            }
        }
        
        //do.cgi --> do
        if (!cgi_name.empty()) {
            auto pos_dot = cgi_name.find_last_of('.');
            if (pos_dot != std::string::npos) {
                std::string name = cgi_name.substr(0, pos_dot);
                auto str3 = mapping_class(name);
                if (!str3.empty()) {
                    return str3;
                }
            }
        }

        return std::string();
    }

    std::shared_ptr<Http_Handle_Base> get_class_instance(const std::string& class_name) {
        //find instance from cache
        auto it = class_instance_.find(class_name);
        if (it != class_instance_.end()) {
            return it->second;
        }

        //try to create an instance
        auto instance = Instance_Factory<Http_Handle_Base>::instance()->create(class_name);
        if (!instance || 0 != instance->init()) {
            return nullptr;
        }

        instance->set_owner_app(this);
        class_instance_[class_name] = instance;
        return instance;
    }

    void clear_all_instance() {
        for (auto & k : class_instance_) {
            k.second.reset();
        }
        class_instance_.clear();
    }

    void clear_timeout_session(bool force=false) {
        time_t now = time(NULL);
        if ( !(force || session_checktime_ + session_check_interval_ >= now) ) {
            return;
        }

        for (auto list_it = session_list_.begin(); list_it != session_list_.end();) {
            if ((*list_it)->access_time() + session_alive_seconds_ + 60 > now ) { //addition 60 seconds
                break;
            }
            //session timeout
            auto map_it = session_map_.find((*list_it)->session_id());
            assert(map_it != session_map_.end());
            session_map_.erase(map_it);
            session_list_.erase(list_it++);
        }
        session_checktime_ = time(NULL);
    }

private:
    unsigned int session_alive_seconds_;
    unsigned int session_check_interval_;   
    time_t       session_checktime_;
    std::string  session_cookie_name_;
    std::string  session_cookie_domain_;
    std::string  session_cookie_path_;
    std::string  session_cookie_comment_;
    std::string  default_handle_class_;
    std::unordered_map<std::string, std::string>                        uri_class_mapping_;//uri -> handle class
    std::unordered_map<std::string, std::shared_ptr<Http_Handle_Base> > class_instance_;   //handle class -> instance

    /* reference::
    * http://www.outofcore.com/2011/04/c-container-iterator-invalidation/
    * http://stackoverflow.com/questions/9722127/remove-element-from-stdmap-based-on-the-time-of-insertion
    */
    typedef std::list<std::shared_ptr<Http_Session> > Session_List;
    typedef std::pair<std::shared_ptr<Http_Session>, Session_List::iterator > Map_Value;
    typedef std::unordered_map<std::string, Map_Value > Session_Map;
    Session_List session_list_;
    Session_Map  session_map_;
};

Http_Session* Http_Handle_Base::fetch_session(Http_Request& req, Http_Response& rsp, bool allow_create)
{
    return this->app_->ensure_session_exists(req, rsp, allow_create);
}

FASTCGI_CPP_NED

#endif



/*
变量名	        描述
-----------------------------------------------------------
CONTENT_TYPE	这个环境变量的值指示所传递来的信息的MIME类型。目前，环境变量CONTENT_TYPE一般都是：application/x-www-form-urlencoded,他表示数据来自于HTML表单。
CONTENT_LENGTH	如果服务器与CGI程序信息的传递方式是POST，这个环境变量即使从标准输入STDIN中可以读到的有效数据的字节数。这个环境变量在读取所输入的数据时必须使用。
HTTP_COOKIE	    客户机内的 COOKIE 内容。
HTTP_USER_AGENT	提供包含了版本数或其他专有数据的客户浏览器信息。
PATH_INFO	    这个环境变量的值表示紧接在CGI程序名之后的其他路径信息。它常常作为CGI程序的参数出现。
QUERY_STRING	如果服务器与CGI程序信息的传递方式是GET，这个环境变量的值即使所传递的信息。这个信息经跟在CGI程序名的后面，两者中间用一个问号'?'分隔。
REMOTE_ADDR	    这个环境变量的值是发送请求的客户机的IP地址，例如上面的192.168.1.67。这个值总是存在的。而且它是Web客户机需要提供给Web服务器的唯一标识，可以在CGI程序中用它来区分不同的Web客户机。
REMOTE_HOST	    这个环境变量的值包含发送CGI请求的客户机的主机名。如果不支持你想查询，则无需定义此环境变量。
REQUEST_METHOD	提供脚本被调用的方法。对于使用 HTTP/1.0 协议的脚本，仅 GET 和 POST 有意义。
SCRIPT_FILENAME	CGI脚本的完整路径
SCRIPT_NAME	    CGI脚本的的名称
SERVER_NAME	    这是你的 WEB 服务器的主机名、别名或IP地址。
SERVER_SOFTWARE	这个环境变量的值包含了调用CGI程序的HTTP服务器的名称和版本号。例如，上面的值为Apache/2.2.14(Unix)

GATEWAY_INTERFACE	运行的CGI版本. 对于UNIX服务器, 这是CGI/1.1.
SERVER_PROTOCOL	服务器运行的HTTP协议. 这里当是HTTP/1.0.
SERVER_PORT	    服务器运行的TCP口，通常Web服务器是80.
REQUEST_METHOD	POST 或 GET, 取决于你的表单是怎样递交的.
HTTP_ACCEPT 	浏览器能直接接收的Content-types, 可以有HTTP Accept header定义.
HTTP_USER_AGENT	递交表单的浏览器的名称、版本 和其他平台性的附加信息。
HTTP_REFERER	递交表单的文本的 URL，不是所有的浏览器都发出这个信息，不要依赖它
PATH_INFO	    附加的路径信息, 由浏览器通过GET方法发出.
PATH_TRANSLATED	在PATH_INFO中系统规定的路径信息.
REMOTE_USER	    递交脚本的用户名. 如果服务器的authentication被激活，这个值可以设置。
REMOTE_IDENT	如果Web服务器是在ident (一种确认用户连接你的协议)运行, 递交表单的系统也在运行ident, 这个变量就含有ident返回值
*/