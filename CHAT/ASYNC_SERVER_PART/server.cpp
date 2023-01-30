#include "server.h"

#include <iostream>
#include <filesystem>
#include <fstream>
#include <ctime>

namespace server {

// Разместите здесь реализацию http-сервера, взяв её из задания по разработке асинхронного сервера
void ReportError(beast::error_code ec, std::string_view what) {
    using namespace std::literals;
    std::cout << what << ": "sv << ec.message() << std::endl;
}

}  // namespace server
