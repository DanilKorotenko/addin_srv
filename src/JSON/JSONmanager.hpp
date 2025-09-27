#pragma once 
#include "JSONstructs.hpp"

class JSONmanager
{
public:
    static JSONstructs::ClassificationLabelData generateClassificationLabels();
    static JSONstructs::XMLClassificationData generateXMLClassificationFonts();

    template<typename T>
    static std::string serialize(const T& JSONstruct);
};