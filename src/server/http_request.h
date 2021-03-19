#ifndef __KIEV_HTTP_REQUEST_H__
#define __KIEV_HTTP_REQUEST_H__

#include <string.h>
#include <stdlib.h>
#include <map>
#include <string>

struct FCGX_Request;

namespace server {

class HttpRequest
{
public:
	enum EnumHttpMethod {
		HTTP_METHOD_GET,
		HTTP_METHOD_POST,
		HTTP_METHOD_HEAD,
		HTTP_METHOD_UNKNOW,
		HTTP_METHOD_ERROR,
	};

	enum HttpSchema {
		SCHEMA_HTTP, SCHEMA_HTTPS, SCHEMA_UNKNOW,
	};

	enum HttpConst {
		MAX_CONTENT_LENGTH = 1024*1024,
	//最大请求参数长度
	};

	explicit HttpRequest(int listen_sock);
	virtual ~HttpRequest(void);

	/**
	 * 查找与参数名匹配的参数。
	 */
    const char* GetParameter(const std::string& param_name);

	int GetParameterCount(void); 

    std::string GetHttpBody();

	/**
	 * 取得请求的HTTP方法
	 */
	EnumHttpMethod GetMethod(void); 

	std::string GetReferer(void) const {
		return GetEnv("HTTP_REFERER");
	}

	std::string GetUserAgent(void) const {
		return GetEnv("HTTP_USER_AGENT");
	}

	/**
	 * The interpreted pathname of the current CGI (relative to the document root)
	 */
	std::string GetScriptName(void) const {
		return GetEnv("SCRIPT_NAME");
	}

	std::string GetQueryString(void) const {
		return GetEnv("QUERY_STRING");
	}

	std::string GetRemoteAddr(void) const {
		return GetEnv("REMOTE_ADDR");
	}

	//	 std::string *GetMethod(void)const {
	//	    return GetEnv("REQUEST_METHOD");
	//	}

	std::string GetServerName(void) const {
		return GetEnv("SERVER_NAME");
	}

	std::string GetServerPort(void) const {
		return GetEnv("SERVER_PORT");
	}

	HttpSchema GetHttpSchema(void) {
		if (m_schema == SCHEMA_UNKNOW) {
            std::string https = GetEnv("HTTPS");
			if (https == "on") {
				m_schema = SCHEMA_HTTPS;
			} else {
				m_schema = SCHEMA_HTTP;
			}
		}
		return m_schema;
	}

    std::string GetContentType(void) const {
		return GetEnv("CONTENT_TYPE");
	}

    std::string GetRequestURI(void) const{
		return GetEnv("REQUEST_URI");
	}

    std::string GetEnvParam(const std::string& paraname)const{
		return GetEnv(paraname);
	}

	void AddParam(const std::string& name, const std::string& value);

    std::string GetEnv(const std::string& env_name) const; 

    FCGX_Request * GetFCGX_Request() { return  request_; }

	//已初始化的HTTP请求
	bool m_initialized;
private:
	/**
	 * 解析url的GET方法的查询字符串。并且做utf8的decode
	 * @return 参数个数
	 */
	int ParseUrlQuery(void);
private:

	HttpRequest(const HttpRequest &hr);

	HttpRequest & operator=(const HttpRequest &hr);

    FCGX_Request *request_;

    std::map<std::string, std::string> params_map_;

    std::string http_body_;

	//请求的HTTP方法
	EnumHttpMethod m_method;
	//HTTP协议
	HttpSchema m_schema;
};
};

#endif /* HTTPREQUESTH */
