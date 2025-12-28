#pragma once

#include <cstddef>
#include "XMLReader.h"
#include "String.h"

namespace Exchange::Gateway {
    class Config : public Core::XMLNode {
        Core::String mPort;
        Core::String mBlockingQueueSize;
        Core::String mMaxFixEventSize;
        Core::String mBacklogSize;

        Config(const tinyxml2::XMLElement* element):XMLNode(element){
            mPort = getChild("Port").get();
            mBlockingQueueSize = getChild("BlockingQueue").getChild("Size").get();
            mMaxFixEventSize = getChild("Fix").getChild("MaxEventSize").get();
            mBacklogSize = getChild("Fix").getChild("BacklogSize").get();
        }
    public:
        // Delete copy and move constructor to enforce singleton
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;

        // Lazy initialization
        static void init(const tinyxml2::XMLElement* element) {
            static Config instance(element);
            getInstance() = &instance;
        }

        // Accessor
        static Config& instance() {
            Config* inst = getInstance();
            if (!inst) {
                ENG_THROW("Gateway::Config accessed before init()");
            }
            return *inst;
        }

        // Getters
        size_t port() const {return std::stoul(mPort.toString());}
        size_t blockingQueueSize() const {return std::stoul(mBlockingQueueSize.toString());}
        size_t maxFixEventSize() const {return std::stoul(mMaxFixEventSize.toString());}
        size_t backlogSize() const {return std::stoul(mBacklogSize.toString());}

    private:
        static Config*& getInstance() {
            static Config* instance = nullptr;
            return instance;
        }
    };
} // namespace Exchange::Gateway