
#include "http_request.h"
#include "url_utils.h"
#include "fcgiapi.h"
#include "url_utils.h"
#include <st/logger.h>

namespace server {

HttpRequest::HttpRequest(int listen_sock) {
	m_initialized = false;
	m_method=HTTP_METHOD_ERROR;
	m_schema = SCHEMA_UNKNOW;
    request_ = (FCGX_Request*)calloc(1, sizeof(*request_));
    FCGX_InitRequest(request_, listen_sock, 0);
}

HttpRequest::~HttpRequest() {
    FCGX_Finish_r(request_); 
    free(request_);
}

const char* HttpRequest::GetParameter(const std::string& param_name){
	if(!m_initialized){//未初始化参数表，读取参数列表
		//解析参数
		ParseUrlQuery();
	}
    std::map<std::string, std::string>::iterator iter;

    iter = params_map_.find(param_name);
    if (iter == params_map_.end()) {
        return NULL;
    }

    return iter->second.c_str();
}	

int HttpRequest::GetParameterCount(void) {

	if (!m_initialized) {//未初始化参数表，读取参数列表
		//解析参数
		ParseUrlQuery();
	}
	return (int)params_map_.size();
}

HttpRequest::EnumHttpMethod HttpRequest::GetMethod(void) {
	if (m_method == HTTP_METHOD_ERROR) {
		std::string httpMethod = GetEnv("REQUEST_METHOD");
		if (httpMethod == "POST") {
			m_method = HTTP_METHOD_POST;
		} else if (httpMethod == "GET") {
			m_method = HTTP_METHOD_GET;
		} else {
			m_method = HTTP_METHOD_UNKNOW;
		}
	}
	return m_method;
}

///获取非x-www-form-urlencoded格式的post请求数据
std::string HttpRequest::GetHttpBody(){ 
    if (!m_initialized) {
        ParseUrlQuery();
    }

    return http_body_;
}


int HttpRequest::ParseUrlQuery(){
	if(m_initialized){
		return 0;
	}

	m_initialized = true;

    std::string content_type = GetContentType();
    if (content_type.empty()) {
        content_type = "application/x-www-form-urlencoded";
    }

	char *param_buffer = NULL;
	if(GetMethod() == HTTP_METHOD_POST){
        if (content_type.find("multipart") != std::string::npos) {
            return 0;
        }
		unsigned long content_len;
        std::string content_len_str = GetEnv("CONTENT_LENGTH");
        content_len = strtoul(content_len_str.c_str(), 0, 10);
		content_len+=1;

		param_buffer = (char*)malloc(sizeof(char)*content_len);
		bzero(param_buffer,sizeof(char)*content_len);
		if(0 > FCGX_GetStr(param_buffer, content_len, 
                           request_->in)) {
            free(param_buffer);
			return -1;
		}

        http_body_ = param_buffer;
	    if(content_type.find("x-www-form-urlencoded") == std::string::npos){
            free(param_buffer);
            return 0;
        }

	}else if(GetMethod() == HTTP_METHOD_GET){
        std::string query_string = GetEnv("QUERY_STRING");
		param_buffer = strdup(query_string.c_str());
	}else{
		//TODO: 不支持的方法
        log_error("not support http method");
		return -1;
	}

	char *query = param_buffer;

	char *q;

	while((q = strsep(&query, "&"))!=NULL){
        char *value = q;
        char *name = strsep(&value, "=");
        if (name == NULL || value == NULL) {
            continue;
        }

        name = url_decode2(name);
        value = url_decode2(value);

        params_map_[name] = value;
	}

	free(param_buffer);
	return 0;
}

void HttpRequest::AddParam(const std::string& name, const std::string& value){
	m_initialized = true;
    params_map_[name] = value;
}

std::string HttpRequest::GetEnv(const std::string& env_name) const {
    char *value = FCGX_GetParam(env_name.c_str(), 
                                request_->envp);
    if (value == NULL) {
        return "";
    }

    return value;
}

};

