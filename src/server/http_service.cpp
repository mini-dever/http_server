#include "http_service.h"
#include "fcgiapi.h"
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>


namespace server {

HttpService::HttpService() {
    init_flag_ = false;
    stats_init_flag_ = false;
    task_num_ = 0;
    cur_task_num_ = 0;
}

HttpService::~HttpService() {
}

/**
 * 初始化服务。调用所有Handler.Init()方法。以及数据源的初始化
 */
int HttpService::Init(const std::string& host, uint16_t port,
                      uint16_t worker_num,
                      uint32_t task_num, uint32_t timeout){

    signal(SIGCHLD, SIG_IGN);
    st_init();
    // task_num已废弃，兼容旧版本接口，此处暂不删除
    if (task_num == 0) {
        task_num = 128;
    }
    task_num_ = task_num;

    // 保持http长连接不失效,timeout不再使用
    if (timeout == 0) {
        timeout = 1000;
    }

    if (init_flag_) {
        log_error("http service has already init");
        return -1;
    }

    listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_ == -1) {
        log_error("create listen socket error: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    if (host.size()) {
        addr.sin_addr.s_addr = inet_addr(host.c_str());
        host_ = host;
    }else{
        addr.sin_addr.s_addr = 0;
        host_ = "";
    }
    addr.sin_port = htons(port);

    int val = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR, (void*)&val, sizeof(val));

    if (-1 == bind(listen_sock_, (struct sockaddr*)&addr, sizeof(addr))) {
        log_error("listen socket bind error: %s", strerror(errno));
        close(listen_sock_);
        return -1;
    }

    if (-1 == listen(listen_sock_, 1024)) {
        log_error("listen error: %s", strerror(errno));
        close(listen_sock_);
        return -1;
    }

    for (uint16_t i = 0; i < worker_num - 1; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            break;
        }
    }
    pid_ = getpid();


    // 为poll传递timeout值
    FCGX_Init(10000);

    init_flag_ = true;
    //st_init(); //在这里初始化只能调用一次协程
	return 0;
}

/**
 * 开始服务，循环Accept。
 */
void HttpService::Start(){

    AcceptRoutine();
}


void HttpService::StartWithoutLoop()
{
    go std::bind(&HttpService::AcceptRoutine, this);
}

/**
 * 请求增加处理器。
 * path_regex 最大长度255 bytes
 * HandlerNode 的生命周期由HttpService接管
 */
int HttpService::AddHandler(const std::string& url, HttpHandler *handler){

    std::map<std::string, HttpHandler*>::iterator iter;

    iter = handler_map_.find(url);
    if (iter != handler_map_.end()) {
        log_error("handler '%s' has already exist", url.c_str());
        return -1;
    }

    handler_map_[url] = handler;

	return 0;
}


void HttpService::AcceptRoutine()
{
    for(;;) {
        HttpRequest *request = new HttpRequest(listen_sock_);
        if (FCGX_Accept_r(request->GetFCGX_Request()) < 0) {
            delete request;
            continue;
        }

        go std::bind(&HttpService::TaskRoutine, this, request);
        cur_task_num_++;
    }
}

void HttpService::TaskRoutine(HttpRequest *request)
{
    while (true) {
        //提前获取rui地址
        url_ = request->GetScriptName();
        if (-1 == FCGX_Process(request->GetFCGX_Request())) {
            return;
        }

        HttpResponse response(request);
        std::map<std::string, HttpHandler*>::iterator iter;
        iter = handler_map_.find(request->GetScriptName());
        if (iter == handler_map_.end()) {
            log_error("Request URL not found: %s", request->GetScriptName().c_str());
            response.SendError(404);
        } else {
            uint64_t begin = st_mstime();
            iter->second->DoService(*request, response);

            //统计调用的平均时间、最大时间
            uint64_t end = st_mstime();
            uint32_t diff = end - begin;

        }
        
        // 如果出现断连的情况，返回1
        if (FCGX_Finish_r(request->GetFCGX_Request()) == 1) {
            delete request;
            cur_task_num_--;
            return;
        } else {
            //未断开连接，刷新参数，继续等待连接
            request->m_initialized = false;
            continue;
        }
    }
}


};
