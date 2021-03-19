#ifndef __KIEV_HTTP_HANDLER_H__
#define __KIEV_HTTP_HANDLER_H__

#include "http_request.h"
#include "http_response.h"

namespace server {

class HttpHandler {

public:
	HttpHandler(){}

	virtual ~HttpHandler(){}

	/**
	 * 每次请求过来，都会执行该函数，处理请求，并生成结果写入response
	 */
	virtual void DoService(HttpRequest &request, HttpResponse &response)=0;

};
};

#endif /* HTTPHANDLER_H_ */
