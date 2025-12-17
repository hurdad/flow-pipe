#pragma once

#include "flow_spec.h"
#include "stage.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace flowpipe {

    class StageRegistry {
    public:
        using Factory =
            std::function<std::shared_ptr<IStage>(const StageSpec&)>;

        void add(const std::string& type, Factory factory) {
            factories_[type] = std::move(factory);
        }

        std::shared_ptr<IStage> create(const StageSpec& spec) const {
            auto it = factories_.find(spec.type);
            if (it == factories_.end()) {
                throw std::runtime_error("Unknown stage type: " + spec.type);
            }
            return it->second(spec);
        }

    private:
        std::unordered_map<std::string, Factory> factories_;
    };

}  // namespace flowpipe
