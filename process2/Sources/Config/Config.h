#pragma once

#include <cstddef>
#include "XMLReader.h"
#include "String.h"
#include "Exception.h"

namespace Exchange::Sequencer {

    class Config : public Core::XMLNode {
    public:
        // Delete copy/move to prevent duplication
        Config(const Config&) = delete;
        Config& operator=(const Config&) = delete;

        struct SeqConfig {
            std::size_t PORT;
            std::size_t BLOCKING_QUEUE_SIZE;
            Core::String IPC_QUEUE_GATEWAY;
            Core::String IPC_QUEUE_ENGINE;
        };

        // Initialize from XML (call once at startup)
        static void init(const tinyxml2::XMLElement* element) {
            if (sInstance) {
                ENG_THROW("Sequencer::Config::init() called twice");
            }
            sInstance = new Config(element); 
        }

        // Accessor
        static const SeqConfig& instance() {
            if (!sInstance) {
                ENG_THROW("Sequencer::Config accessed before init()");
            }
            return sInstance->mConfig;
        }
        
        static void shutdown() {
            delete sInstance;
            sInstance = nullptr;
        }
        
    private:
        SeqConfig mConfig;
        Config(const tinyxml2::XMLElement* element):XMLNode(element){
            mConfig.PORT = std::stoul(getChild("Port").get().toString());
            mConfig.BLOCKING_QUEUE_SIZE = std::stoul(getChild("BlockingQueue").getChild("Size").get().toString());
            mConfig.IPC_QUEUE_GATEWAY = getChild("Ipc").getChild("SequencerQueue").get();
            mConfig.IPC_QUEUE_ENGINE = getChild("Ipc").getChild("MatchingEngineQueue").get();
        }
        static Config* sInstance;
    };
    // Static member definition
    inline Config* Config::sInstance = nullptr;
}