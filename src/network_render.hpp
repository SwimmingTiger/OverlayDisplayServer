#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <Indicium/Engine/IndiciumCore.h>
#include <json11.hpp>

#include "lua_vm.hpp"

using namespace std;
using namespace json11;
using namespace websocketpp::frame;

using websocketServer = websocketpp::server<websocketpp::config::asio>;
using message_ptr = websocketServer::message_ptr;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using websocketpp::connection_hdl;


class NetworkRender {
protected:
    websocketServer server_;
    unordered_map<string, LuaVM> luaVMs_;
    mutex luaVMLock_;

    NetworkRender() {
    }

    ~NetworkRender() {
    }

public:
    static NetworkRender& getInstance() {
        static NetworkRender instance;
        return instance;
    }

    // Define a callback to handle incoming messages
    void OnMessage(connection_hdl hdl, message_ptr msg) {

        try {
            string err;
            const auto json = Json::parse(msg->get_payload(), err);
            if (!err.empty()) {
                ResponseError(hdl, "", 400, "parse json failed: " + err);
                return;
            }

            string id = json["id"].string_value();

            string widget = json["widget"].string_value();
            if (widget.empty()) {
                ResponseError(hdl, id, 400, "missing field 'widget'");
                return;
            }

            // Got a lock before access luaVMs_
            lock_guard<mutex> scopeLock(luaVMLock_);

            auto itr = luaVMs_.find(widget);
            if (itr == luaVMs_.end()) {
                bool ok = luaVMs_[widget].Init();
                if (!ok) {
                    ResponseError(hdl, id, 500, "LuaVM init failed: " + luaVMs_[widget].GetResponse());
                    luaVMs_.erase(widget);
                    return;
                }
            }

            string command = json["command"].string_value();
            if (!command.empty()) {
                if (command == "remove") {
                    luaVMs_.erase(widget);
                    Response(hdl, id, 200, "ok");
                    return;
                }

                if (command == "get_response") {
                    LuaVM& vm = luaVMs_[widget];
                    Response(hdl, id, 200, vm.GetResponse());
                    vm.ClearResponse();
                    return;
                }

                ResponseError(hdl, id, 404, "unknown command '"+command+"'");
                return;
            }

            LuaVM& vm = luaVMs_[widget];

            string code = json["code"].string_value();
            if (!code.empty()) {
                bool ok = vm.SetCode(std::move(code));
                if (!ok) {
                    ResponseError(hdl, id, 500, "load code failed: " + vm.GetResponse());
                    return;
                }
                Response(hdl, id, 200, "ok");
                return;
            }

            ResponseError(hdl, id, 400, "request should contains one of these fields: 'command' or 'code'");
            return;
        }
        catch (websocketpp::exception const& e) {
            IndiciumEngineLogInfo((string("websocketpp exception: ") + e.what()).c_str());
        }
        catch (std::exception const& e) {
            IndiciumEngineLogInfo((string("NetworkRender exception: ") + e.what()).c_str());
        }
        catch (...) {
            IndiciumEngineLogInfo("NetworkRender exception: unknown");
        }
    }

    void Response(connection_hdl hdl, string id, int code, string&& msg) {
        try {
            Json errObj = Json::object{
                { "id", id },
                { "response", Json::object{
                              { "code", code },
                              { "payload", msg },
                           }
                }
            };
            server_.send(hdl, errObj.dump(), opcode::text);
        }
        catch (websocketpp::exception const& e) {
            IndiciumEngineLogInfo((string("websocketpp exception: ") + e.what()).c_str());
        }
        catch (std::exception const& e) {
            IndiciumEngineLogInfo((string("NetworkRender exception: ") + e.what()).c_str());
        }
        catch (...) {
            IndiciumEngineLogInfo("NetworkRender exception: unknown");
        }
    }

    void ResponseError(connection_hdl hdl, string id, int code, string &&msg) {
        try {
            Json errObj = Json::object{
                { "id", id },
                { "error", Json::object{
                              { "code", code },
                              { "message", msg },
                           }
                }
            };
            server_.send(hdl, errObj.dump(), opcode::text);
        }
        catch (websocketpp::exception const& e) {
            IndiciumEngineLogInfo((string("websocketpp exception: ") + e.what()).c_str());
        }
        catch (std::exception const& e) {
            IndiciumEngineLogInfo((string("NetworkRender exception: ") + e.what()).c_str());
        }
        catch (...) {
            IndiciumEngineLogInfo("NetworkRender exception: unknown");
        }
    }

    bool Run(uint16_t bindPort) {
        try {
            // Set logging settings
            server_.set_access_channels(websocketpp::log::alevel::all);
            server_.clear_access_channels(websocketpp::log::alevel::frame_payload);

            // Initialize Asio
            server_.init_asio();

            // Register our message handler
            server_.set_message_handler(bind(&NetworkRender::OnMessage, this, ::_1, ::_2));

            // Listen on port 9002
            server_.listen(bindPort);

            // Start the server accept loop
            server_.start_accept();

            // Start the ASIO io_service run loop
            server_.run();
            return true;
        }
        catch (websocketpp::exception const& e) {
            IndiciumEngineLogInfo((string("websocketpp exception: ") + e.what()).c_str());
        }
        catch (std::exception const& e) {
            IndiciumEngineLogInfo((string("NetworkRender exception: ") + e.what()).c_str());
        }
        catch (...) {
            IndiciumEngineLogInfo("NetworkRender exception: unknown");
        }
        return false;
    }

    void Stop() {
        server_.stop();
    }

    void Render() {
        // Got a lock before access luaVMs_
        lock_guard<mutex> scopeLock(luaVMLock_);

        for (auto& item : luaVMs_) {
            item.second.RunCode();
        }
    }
};
