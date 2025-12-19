#include <fstream>
#include <iostream>
#include <string>

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

#include "flowpipe/runtime.h"
#include "flowpipe/v1/flow.pb.h"

using json = nlohmann::json;

static bool load_from_yaml(const std::string &path,
                           flowpipe::v1::FlowSpec &flow) {
    YAML::Node root = YAML::LoadFile(path);

    if (!root["queues"] || !root["stages"]) {
        std::cerr << "yaml: missing queues or stages\n";
        return false;
    }

    for (const auto &q: root["queues"]) {
        auto *queue = flow.add_queues();
        queue->set_name(q["name"].as<std::string>());
        queue->set_capacity(q["capacity"].as<uint32_t>());
    }

    for (const auto &s: root["stages"]) {
        auto *stage = flow.add_stages();
        stage->set_name(s["name"].as<std::string>());
        stage->set_type(s["type"].as<std::string>());
        stage->set_threads(s["threads"].as<uint32_t>());

        if (s["plugin"])
            stage->set_plugin(s["plugin"].as<std::string>());
        if (s["input_queue"])
            stage->set_input_queue(s["input_queue"].as<std::string>());
        if (s["output_queue"])
            stage->set_output_queue(s["output_queue"].as<std::string>());

        if (s["params"]) {
            for (const auto &it: s["params"]) {
                (*stage->mutable_params())[it.first.as<std::string>()]
                        .set_string_value(it.second.as<std::string>());
            }
        }
    }

    return true;
}

static bool load_from_json(const std::string &path,
                           flowpipe::v1::FlowSpec &flow) {
    std::ifstream in(path);
    if (!in) return false;

    json root;
    in >> root;

    if (!root.contains("queues") || !root.contains("stages")) {
        std::cerr << "json: missing queues or stages\n";
        return false;
    }

    for (const auto &q: root["queues"]) {
        auto *queue = flow.add_queues();
        queue->set_name(q.at("name").get<std::string>());
        queue->set_capacity(q.at("capacity").get<uint32_t>());
    }

    for (const auto &s: root["stages"]) {
        auto *stage = flow.add_stages();
        stage->set_name(s.at("name").get<std::string>());
        stage->set_type(s.at("type").get<std::string>());
        stage->set_threads(s.at("threads").get<uint32_t>());

        if (s.contains("plugin"))
            stage->set_plugin(s.at("plugin").get<std::string>());
        if (s.contains("input_queue"))
            stage->set_input_queue(s.at("input_queue").get<std::string>());
        if (s.contains("output_queue"))
            stage->set_output_queue(s.at("output_queue").get<std::string>());

        if (s.contains("params")) {
            for (auto it = s["params"].begin(); it != s["params"].end(); ++it) {
                (*stage->mutable_params())[it.key()]
                        .set_string_value(it.value().get<std::string>());
            }
        }
    }

    return true;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: flow_runtime <flow.yaml|flow.json>\n";
        return 1;
    }

    const std::string path = argv[1];
    flowpipe::v1::FlowSpec flow;

    bool ok = false;
    if (path.ends_with(".yaml") || path.ends_with(".yml")) {
        ok = load_from_yaml(path, flow);
    } else if (path.ends_with(".json")) {
        ok = load_from_json(path, flow);
    } else {
        std::cerr << "unsupported file type (use yaml or json)\n";
        return 1;
    }

    if (!ok) {
        std::cerr << "failed to load flow config\n";
        return 1;
    }

    flowpipe::Runtime runtime;
    return runtime.run(flow);
}
