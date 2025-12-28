#pragma once

#include <string>
#include <stdexcept>
#include "tinyxml2.h"
#include "String.h"
#include "Exception.h"

namespace Exchange::Core {

    class XMLNode {
    public:
        explicit XMLNode(const tinyxml2::XMLElement* element = nullptr)
            : mElement(element) {}

        const tinyxml2::XMLElement* mElement;
        bool isValid() const {
            return mElement != nullptr;
        }

        XMLNode getChild(const String& name) const {
            if (!isValid()) {
                ENG_THROW("Attempt to access child of null XmlNode");
            }

            const tinyxml2::XMLElement* child = mElement->FirstChildElement(name);
            if (!child) {
                ENG_THROW(String("Missing required XML element: ") + name);
            }

            return XMLNode(child);
        }
    
        String get() const {
            if (!mElement || !mElement->GetText()) {
                ENG_THROW("XML element has no text");
            }
            return mElement->GetText();
        }
    };
}