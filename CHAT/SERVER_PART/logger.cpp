#include "logger.h"
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>


namespace logger{
    namespace logging = boost::log;
    namespace expr = logging::expressions;
    namespace keywords = logging::keywords;

    void MyFormatter(logging::record_view const& rec, logging::formatting_ostream& strm) {
        // чтобы поставить логеры в равные условия, уберём всё лишнее
        auto ts = rec[timestamp];
        strm << "{";
        strm << "\"timestamp\":\"" << to_iso_extended_string(*ts) << "\",";
        auto data_json = rec[data];
        strm << "\"data\":";
        auto json = data_json.get().as_object();
        strm << json << ",";
        strm << "\"message\":\"" << rec[expr::smessage] << "\"}";
    }

    // Выведите в поток все аргументы.
    void Logger::Log(const std::string_view message, boost::json::value data_json){
        BOOST_LOG_TRIVIAL(info) << logging::add_value(data, data_json) << message;
    }

    void Logger::InitBoostLog(){
        //setting up logging
        logging::add_common_attributes();
        logging::add_console_log(
            std::cout,
            keywords::format = &logger::MyFormatter,
	    keywords::auto_flush = true
        );
    }
}//namespace logger
