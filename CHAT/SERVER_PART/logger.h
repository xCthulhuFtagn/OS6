#pragma once

#include <iostream>
#include <string>

#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/date_time.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>

#define LOG_INIT() logger::Logger::GetInstance().InitBoostLog();

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(data, "data", boost::json::value)

using namespace std::literals;


namespace logger{

    void MyFormatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm);

    class Logger {
        Logger() = default;
        Logger(const Logger&) = delete;

    public:
        static Logger& GetInstance() {
            static Logger obj;
            return obj;
        }

        // Выведите в поток все аргументы.
        void Log(const std::string_view, boost::json::value);

        void InitBoostLog();
    };

}//namespace logger