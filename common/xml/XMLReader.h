#pragma once

#include <string>
#include <stdexcept>
#include "tinyxml2.h"
#include "XMLNode.h"
#include "String.h"

namespace Exchange::Core {

    class XMLReader {
        const Core::String gExchange = "Exchange";

        tinyxml2::XMLDocument mDoc;
        const tinyxml2::XMLElement* mRoot;
    public:
        explicit XMLReader(const String& fileName) {
            if (mDoc.LoadFile(fileName) != tinyxml2::XML_SUCCESS) {
                ENG_THROW("Failed to load XML file: %ls", fileName.get());
            }

            mRoot = mDoc.FirstChildElement(gExchange);
            if (!mRoot) {
                ENG_THROW("Missing <Exchange> root node");
            }
        }

        auto getNode(String name) {
            auto node = mRoot->FirstChildElement(name);
            if (!node) {
                ENG_THROW("Missing <%ls> node", name);
            }
            return node;
        }
    };
} // namespace Exchange::Core