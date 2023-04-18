# Создать образ на основе базового слоя gcc (там будет ОС и сам компилятор).
# 11.3 — используемая версия gcc.
FROM gcc:11.3

# Выполнить установку зависимостей внутри контейнера.
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install -U conan

# Скопировать файлы проекта внутрь контейнера
COPY logger.h logger.cpp main.cpp CMakeLists.txt conanfile.txt /

# новая команда для сборки сервера:
RUN mkdir /build && cd /build
RUN conan profile detect
RUN conan install .. -s build_type=Debug
RUN cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . 