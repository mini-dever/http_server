#ifndef __KIEV_HTTP_RESPONSE_H__
#define __KIEV_HTTP_RESPONSE_H__

#include "http_request.h"

namespace server {

class HttpResponse 
{
public:
    HttpResponse(HttpRequest * request);
    virtual ~HttpResponse();

    /**
     * 设置回应报文的头部。
     */
    int SetHeader(const std::string& name, const std::string& value);
    /**
     * 写应答数据到HTTP输出流。
     */
    int Write(const char *text, ...);

    /**
     * 往HTTP流写数据。
     *
     * 返回 成功写入的字节数
     */
    size_t WriteByte(const void *buf, size_t buf_size);

    /**
     * 直接发送错误码给客户端。
     */
    int SendError(int status);

    /**
     * 302重定向
     */
    int SendRedirection(const std::string& url);

    int Printf(const char * format, ...);

protected:
    /**
     * 发送设置好的HTTP头部。
     */
    int SendHeader();
protected:
    HttpResponse(const HttpResponse &hr);
    HttpResponse & operator=(const HttpResponse &hr);
    HttpRequest * request_;
    std::map<std::string, std::string> header_map_;
    bool m_header_sended;                  // 是否已经送出了HTTP头部, 如果已发出，则不允许在修改HTTP头。
};
};

#endif  // _HTTP_RESPONSE_R_H__
