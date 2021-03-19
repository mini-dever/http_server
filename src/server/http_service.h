#ifndef __KIEV_HTTP_SERVICE_H__
#define __KIEV_HTTP_SERVICE_H__

#include "http_handler.h"
#include <st/st.hpp>
#include <map>
#include <queue>
#include <stack>
#include <string>

namespace server {


class HttpService {
public:
	HttpService();
	~HttpService();

	/**
	 * 初始化服务。会调用所有Handler.Init()方法。
	 */
	int Init(const std::string& host, uint16_t port, 
             uint16_t worker_num, 
             uint32_t task_num = 128, uint32_t timeout = 1000);

	/**
	 * 开始服务，循环Accept。
	 */
	void Start();

	/**
	 * 开始服务, 但是不进入循环
	 */
    void StartWithoutLoop();
    void Recvs();
    void Listen();
	/**
	 * 请求增加处理器。
	 */
	int AddHandler(const std::string& url, HttpHandler *handler);

protected:
    void TaskRoutine(HttpRequest *request);

    void AcceptRoutine();

private:
    std::map<std::string, HttpHandler*> handler_map_;
    bool init_flag_;
    bool stats_init_flag_;
    uint16_t enable_fcgi_stats_;

    int listen_sock_;

    uint32_t task_num_;

    uint32_t cur_task_num_;



    //记录当前的host、pid以及传入的url，供统计使用
    std::string host_;
    uint16_t pid_;
    std::string url_;


    void ConfWatchCallback(std::string key, std::string old_value, std::string new_value);
    //动态监控的回调函数

    void StatsLoop();   //统计的loop
    void StatsStart();   //统计初始化需要传一个init_flag

};

};
#endif /* HTTPSERVICE_H_ */
