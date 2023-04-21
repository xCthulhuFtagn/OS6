# Создать образ на основе базового слоя gcc (там будет ОС и сам компилятор).
# 11.3 — используемая версия gcc.
# Не просто создаём образ, но даём ему имя build
FROM gcc:11.3 as build

# Выполнить установку зависимостей внутри контейнера.
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.59

# Копируем conanfile.txt в контейнер и запускаем conan install
COPY conanfile.txt /app/
RUN mkdir /app/build && cd /app/build && \
    conan install .. -s compiler.libcxx=libstdc++11 -s build_type=Debug --build=missing

# Только после этого копируем остальные иходники
COPY    server.cpp \
        logger.h logger.cpp \
        server_interface.h server_interface.cpp \
        server_routines.h server_routines.cpp\
        CMakeLists.txt \
    /app/

RUN cd /app/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Debug && \
    cmake --build . 

# Второй контейнер в том же докерфайле
FROM ubuntu:22.04 as run

# # Создадим пользователя usr
# RUN groupadd -r usr && useradd -r -g usr usr
# USER usr

# Скопируем приложение со сборочного контейнера в директорию /app.
COPY --from=build /app/build/bin/server /app/

# Запускаем тестировщик
ENTRYPOINT ["/app/server"]