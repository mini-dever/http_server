#include <iostream>
#include "./server/http_service.h"
#include "TestHandler.h"
#include "http_handler.h"
#include "http_request.h"
#include "http_response.h"
class NewHandler : public server::HttpHandler{
    void DoService(server::HttpRequest &request, server::HttpResponse &response){
        response.Write("{\"code\":200,\"msg\":\"Server Ok!\"}");
    }
};


int main(int argc,char *argv[]) {

    server::HttpService http_service_;
    http_service_.Init("",8099,0);
    http_service_.AddHandler("/just/test",new NewHandler);
//    while(1){
//        http_service_.Listen();
//    }
    http_service_.StartWithoutLoop();
    for(;;) {
        st_sleep(1);
    }
    return 0;
}
