# Импортируем три модуля:
# * json - для JSON,
# * sys - для чтения из stdin,
# * prometheus_client для работы с Prometheus.
# Последнему для краткости дадим имя prom.
# Это аналог конструкции из C++:
# namespace prom = prometheus_client;
import prometheus_client as prom
import sys
import json

# Создаём метрики
good_lines = prom.Counter('webexporter_good_lines', 'Good JSON records')
wrong_lines = prom.Counter('webexporter_wrong_lines', 'Wrong JSON records')
read_errors = prom.Counter('client_read_errors', 'Client read errors')

response_time = prom.Histogram('webserver_request_duration', 'Response time', labelnames=['client_request_e', 'server_response_e'],
    buckets=(.500, 1.0, 1.5, 2.0, 2.5, 3.0, float("inf")))

# Определим функцию, которую мы будем использовать как main:
def my_main():
    prom.start_http_server(9200)

    # читаем в цикле строку из стандартного ввода
    for line in sys.stdin:
        try:
            data = json.loads(line)

            # Ошибка, если из JSON прочитан не объект
            if not isinstance(data, dict):
                raise ValueError()

            # Регистрирум запрос
            if data["message"] == "Read first response from server":
                total_time_seconds = data["data"]["response time"] / 1000
                response_time.labels(client_request_e=data["data"]["response"]["client_request_e"], server_response_e=data["data"]["response"]["server_response_e"]).observe(total_time_seconds)

            if data["message"] == "Client read error":
                read_errors.inc()

            # Регистрирум успешно разобранную строку
            good_lines.inc()
        except (ValueError, KeyError):
            # Если мы пришли сюда, то при разборе JSON
            # обнаружилась синтаксическая ошибка.
            # Увеличим соотв. счётчик
            wrong_lines.inc()

# Так на самом деле выглядит точка входа в Pyhton.
# Просто вызовем в ней наш аналог функции main.
if __name__ == '__main__':
    my_main()