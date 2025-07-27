#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "../json/serialization.hpp"


struct SendData {
    std::vector<std::string> names;

    PBNJSON_SERIALIZE( SendData,
        (std::vector<std::string>, names)
    )
};

int main(int argc, char const *argv[])
{
    SendData sendArray;
    sendArray.names = {
        "Defualt",
        "Resurected",
        "Listed",
        "Hollowed",
        "Pers"
    };

    httplib::SSLServer svr("cert.pem", "key.pem");
    std::string jsonStr;
    pbnjson::pbnjson_serialize(sendArray, jsonStr);
    svr.Get("/list", [&](const httplib::Request &, httplib::Response &res) {
        res.set_content(jsonStr, "application/json");
        res.set_header("Access-Control-Allow-Origin","https://localhost:3000");
    });
    std::cout << "Server Started!" << std::endl;
    svr.listen("0.0.0.0", 443);
    return 0;
}
