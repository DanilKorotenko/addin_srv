#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "../json/serialization.hpp"
#include <fstream>
#include <sstream>
#include <string>


struct SendData {
    std::vector<std::string> names;

    PBNJSON_SERIALIZE( SendData,
        (std::vector<std::string>, names)
    )
};

std::string load_file(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "<h1>File not found</h1>";

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

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
    svr.set_mount_point("/", "officeAddin/src");

    svr.set_logger([](const httplib::Request &req, const httplib::Response &res) {
        std::cout << req.method << " " << req.path << " -> " << res.status << std::endl;
    });

    std::cout << "Server Started!" << std::endl;
    svr.listen("0.0.0.0", 443);
    return 0;
}
