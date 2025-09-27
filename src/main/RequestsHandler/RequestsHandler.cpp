#include "RequestsHandler.hpp"

RequestsHandler::RequestsHandler()
{
    JSONstructs::ClassificationLabelData a = JSONmanager::generateClassificationLabels();
    JSONstructs::XMLClassificationData b = JSONmanager::generateXMLClassificationFonts();
    _classificationLabels = JSONmanager::serialize(a);
    _XMLClassificationFonts = JSONmanager::serialize(b);
}

void RequestsHandler::getClassificationLabels(const httplib::Request &req, httplib::Response &res)
{
    res.set_content(_classificationLabels, "application/json");
    res.set_header("Access-Control-Allow-Origin","https://192.168.128.4:443");

}

void RequestsHandler::getClassificationFonts(const httplib::Request &req, httplib::Response &res)
{
    res.set_content(_XMLClassificationFonts, "application/json");
    res.set_header("Access-Control-Allow-Origin","https://192.168.128.4:443");
    return;
}
