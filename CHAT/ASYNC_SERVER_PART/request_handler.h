#pragma once
#include "http_server.h"
#include <iostream>
#include "model.h"

namespace http_handler {
namespace beast = boost::beast;
namespace http = beast::http;
using namespace std::literals;
// Запрос, тело которого представлено в виде строки
using StringRequest = http::request<http::string_body>;
// Ответ, тело которого представлено в виде строки
using StringResponse = http::response<http::string_body>;

struct ContentType {
    ContentType() = delete;
    constexpr static auto TEXT_HTML = boost::string_view("text/html");
    constexpr static auto APP_JSON = boost::string_view("application/json");
    // При необходимости внутрь ContentType можно добавить и другие типы контента
};


StringResponse MakeStringResponse(http::status status, std::string_view body, unsigned http_version,
                                    bool keep_alive,
                                    boost::string_view content_type);

class RequestHandler {
public:
    explicit RequestHandler() {}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Обработать запрос request и отправить ответ, используя send
        std::string ans_body;
        StringResponse resp;
        switch(req.method()){
            case http::verb::get:
                if(req.target().find("/api/v1/maps") == 0){
                    auto id = req.target().substr(12); //length of /api/v1/maps -1
                    // std::cout << req.target() << " " << id << std::endl;
                    if(id.empty() || id.size() == 1){
                        auto maps = game_.GetMaps();
                        ans_body += "[";
                        for(auto it = maps.begin(); it < maps.end(); ++it){
                            ans_body += "\n  {\"id\": ";
                            ans_body += "\""s + *it->GetId() + "\", \"name\": ";
                            ans_body += "\""s + it->GetName() + "\"}";
                            if(next(it) != maps.end()) ans_body += ", ";
                        }
                        ans_body += "\n]";
                        resp = MakeStringResponse(http::status::ok, ans_body, req.version(), req.keep_alive(), ContentType::APP_JSON);
                    }
                    else{
                        id.remove_prefix(1); // getting rid of slash
                        if(auto map = game_.FindMap(model::Map::Id(id.data()))){
                            ans_body += "{\n";
                            //id
                            ans_body += "  \"id\": \""s + *map->GetId() + "\",\n";
                            //name
                            ans_body += "  \"name\": \""s + map->GetName() + "\",\n";
                            //roads
                            ans_body += "  \"roads\": [";
                            auto roads = map->GetRoads();
                            for(auto it = roads.begin(); it < roads.end(); ++it){
                                ans_body += "\n    { ";
                                auto start = it->GetStart();
                                ans_body += "\"x0\": " + std::to_string(start.x) + ", ";
                                ans_body += "\"y0\": " + std::to_string(start.y) + ", ";
                                if(it->IsVertical()) ans_body += "\"y1\": " + std::to_string(it->GetEnd().y) + " }";
                                else ans_body += "\"x1\": " + std::to_string(it->GetEnd().x) + " }";
                                if(next(it) != roads.end()) ans_body += ",";
                            }
                            ans_body += "\n  ],\n";
                            //buildings
                            ans_body += "  \"buildings\": [";
                            auto buildings = map->GetBuildings();
                            for(auto it = buildings.begin(); it < buildings.end(); ++it){
                                ans_body += "\n    { ";
                                auto bounds = it->GetBounds();
                                ans_body += "\"x\": " + std::to_string(bounds.position.x) + ", ";
                                ans_body += "\"y\": " + std::to_string(bounds.position.y) + ", ";
                                ans_body += "\"w\": " + std::to_string(bounds.size.width) + ", ";
                                ans_body += "\"h\": " + std::to_string(bounds.size.height) + " }";
                                if(next(it) != buildings.end()) ans_body += ",";
                            }
                            ans_body += "\n  ],\n";
                            //offices
                            ans_body += "  \"offices\": [";
                            auto offices = map->GetOffices();
                            for(auto it = offices.begin(); it < offices.end(); ++it){
                                ans_body += "\n    { ";
                                ans_body += "\"id\": \"" + *it->GetId() + "\", ";
                                ans_body += "\"x\": " + std::to_string(it->GetPosition().x) + ", ";
                                ans_body += "\"y\": " + std::to_string(it->GetPosition().y) + ", ";
                                ans_body += "\"offsetX\": " + std::to_string(it->GetOffset().dx) + ", ";
                                ans_body += "\"offsetY\": " + std::to_string(it->GetOffset().dy) + " }";
                                if(next(it) != offices.end()) ans_body += ",";
                            }
                            ans_body += "\n  ]\n";
                            //end
                            ans_body += "}";
                            resp = MakeStringResponse(http::status::ok, ans_body, req.version(), req.keep_alive(), ContentType::APP_JSON);
                        }
                        else{
                            ans_body = "{\n  \"code\": \"mapNotFound\",\n  \"message\": \"Map not found\"\n}";
                            resp = MakeStringResponse(http::status::not_found, ans_body, req.version(), req.keep_alive(), ContentType::APP_JSON);
                        }
                    }
                }
                else{
                    ans_body = "{\n  \"code\": \"badRequest\",\n  \"message\": \"Bad request\"\n}";
                    resp = MakeStringResponse(http::status::bad_request, ans_body, req.version(), req.keep_alive(), ContentType::APP_JSON);
                }
                break;
            default:
                ans_body = "{\n  \"code\": \"badRequest\",\n  \"message\": \"Bad request\"\n}";
                resp = MakeStringResponse(http::status::bad_request, ans_body, req.version(), req.keep_alive(), ContentType::APP_JSON);
        }
        send(resp);
    }

private:
    model::Game& game_;
};

}  // namespace http_handler
