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

struct FontData {
    std::string fontName;
    std::string fontColor;
    std::string fontSize;
    std::string text;

    PBNJSON_SERIALIZE( FontData,
        (std::string, fontName)
        (std::string, fontColor)
        (std::string, fontSize)
        (std::string, text)
    )
};

struct WaterMarkData {
    std::string fontName;
    std::string fontColor;
    std::string fontSize;
    std::string rotation;
    std::string transparency;
    std::string text;

    PBNJSON_SERIALIZE( WaterMarkData,
        (std::string, fontName)
        (std::string, fontColor)
        (std::string, fontSize)
        (std::string, rotation)
        (std::string, transparency)
        (std::string, text)
    )
};

struct ClassifData {
    std::vector<FontData> hdr;
    std::vector<FontData> ftr;
    WaterMarkData wm;

    PBNJSON_SERIALIZE( ClassifData,
        (std::vector<FontData>, hdr)
        (std::vector<FontData>, ftr)
        (WaterMarkData, wm)
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

ClassifData generateJSON()
{
    ClassifData resultJson;
    FontData hdr;
    FontData ftr;
    WaterMarkData wm;
    std::vector<FontData*> fontDatas;
    fontDatas.push_back(&hdr);
    fontDatas.push_back(&ftr);

    for (FontData *pointer: fontDatas)
    {
        pointer->fontColor = "0000FF";
        pointer->fontSize = "14";
        pointer->fontName = "Arial";
        pointer->text = "SECRET";
    }
    wm.fontColor = "0000FF";
    wm.fontSize = "14";
    wm.fontName = "Arial";
    wm.rotation = "45";
    wm.transparency = "5000";
    wm.text = "SECRET";

    resultJson.hdr.push_back(hdr);
    resultJson.ftr.push_back(ftr);
    resultJson.wm = wm;

    return resultJson;
}

//int main(int argc, char const *argv[])
int main()
{
    SendData sendArray;
    sendArray.names = {
        "Defualt",
        "Resurected",
        "Listed",
        "Hollowed",
        "Pers"
    };

    ClassifData classifJSON = generateJSON();

// TODO:
    std::string jsonFont;
    pbnjson::pbnjson_serialize(classifJSON, jsonFont);
// Do you see this? The variable with name "jsonFont" contains object classifJSON
// So what do we have? font or some classif?

    std::cout << jsonFont << std::endl;

    httplib::SSLServer svr("cert.pem", "key.pem");
    std::string jsonStr;
    pbnjson::pbnjson_serialize(sendArray, jsonStr);

    // TODO: list of what?
    svr.Get("/list", [&](const httplib::Request &, httplib::Response &res) {
        res.set_content(jsonStr, "application/json");
        res.set_header("Access-Control-Allow-Origin","https://192.168.128.4:443");
    });

    // TODO: No shortcuts.
    // What is /list and what is /classiflist ?
    svr.Get("/classiflist", [&](const httplib::Request &, httplib::Response &res) {
        res.set_content(jsonFont, "application/json");
        res.set_header("Access-Control-Allow-Origin","https://192.168.128.4:443");
    });

    // TODO: move assets folder into src folder
    svr.set_mount_point("/", "officeAddin/src");
    svr.set_mount_point("/assets", "officeAddin/assets");

    svr.set_logger([](const httplib::Request &req, const httplib::Response &res) {
        std::cout << "[" << req.remote_addr << "] " << req.method << " " << req.path << " -> " << res.status << std::endl;
    });

    std::cout << "Server Started!" << std::endl;
    svr.listen("0.0.0.0", 443);
    return 0;
}
