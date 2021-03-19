#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "fcgiapi.h"
#include "http_response.h"

namespace server {

HttpResponse::HttpResponse(HttpRequest * request) :
        request_(request),
        m_header_sended(false)
{
}

HttpResponse::~HttpResponse() {
}

int HttpResponse::Write(const char * fmt, ...)
{
    if(!m_header_sended) {
        SendHeader();
    }
    va_list args;
    va_start(args, fmt);
    int count = FCGX_VFPrintF(request_->GetFCGX_Request()->out, fmt, args);
    va_end(args);
    return count;
}

size_t HttpResponse::WriteByte(const void *buf, size_t buf_size){
    if(!m_header_sended){
        SendHeader();
    }
    return FCGX_PutStr((const char *)buf, (int)buf_size, request_->GetFCGX_Request()->out );
}

int HttpResponse::Printf(const char * format, ...)
{
    int result;
    va_list args;
    va_start(args, format);
    result = FCGX_VFPrintF(request_->GetFCGX_Request()->out, format, args);
    va_end(args);
    return result;
}

int HttpResponse::SetHeader(const std::string& name, const std::string& value)
{
    if(m_header_sended){
        return -1; // 已发送
    }

    header_map_[name] = value;
    return 0;
}

int HttpResponse::SendHeader(){
    m_header_sended = true;
    int count = 0;
    int sended_count = 0;
    std::map<std::string, std::string>::iterator iter = header_map_.begin();

    for(; iter != header_map_.end(); iter++) {
        sended_count = Printf("%s:%s\n", iter->first.c_str(),
                              iter->second.c_str());
        if(sended_count<0){
            return -1;
        }
        count += sended_count;
    }

    if(sended_count<0)  {
        return -1;
    }
    if(sended_count==0){
        Printf("%s:%s\n", "Content-Type", "text/html;charset=utf8");
    }

    sended_count = Printf("\n");
    return count + sended_count;
}

int HttpResponse::SendError(int status){
    if(m_header_sended){
        return -1;//头部已经发送，则报错。
    }
    switch(status){
    case 401:
        Printf("Status: 401 Unauthorized\r\n\r\n"
                "<html><head><title>401 Unauthorized</title></head><body>401 Unauthorized</body></html>");
        break;
    case 403:
        Printf("Status: 403 Forbidden\r\n\r\n"
                "<html><head><title>403 Forbidden</title></head><body>403 Forbidden</body></html>");
        break;
    case 404:
        Printf("Status: 404 Not Found\r\n\r\n"
                "<html><head><title>404 Not Found</title></head><body>404 Not Found</body></html>");
        break;
    case 405:
        Printf("Status: 405 Method Not Allowed\r\n\r\n"
                "<html><head><title>405 Method Not Allowed</title></head><body>405 Method Not Allowed</body></html>");
        break;
    case 414:
        Printf("Status: 414 Request-URI Too Long\r\n\r\n"
                "<html><head><title>414 Request-URI Too Lon</title></head><body>414 Request-URI Too Lon</body></html>");
        break;
    case 400:
    default:
        Printf("Status: 400 Bad Request\r\n\r\n"
                "<html><head><title>400 Bad Request</title></head><body>400 Bad Request</body></html>");
        break;
    }
    return 0;
}

int HttpResponse::SendRedirection(const std::string& url){
    if(m_header_sended || url.empty()){
        return -1;  //  头部已经发送，则报错。
    }
    //SetHeader("Location", url);
    Printf("HTTP/1.1 302 Found\r\n"
           "Location: %s\r\n\r\n", url.c_str());
    return 0;
}

};

