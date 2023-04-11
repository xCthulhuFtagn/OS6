$ mkdir build_debug && cd build_debug

# Под Linux обязательно нужно указывать параметр
# -s compiler.libcxx=libstdc++11. Иначе собранная
# программа будет падать:
$ conan install .. -s compiler.libcxx=libstdc++11 -s build_type=Debug

$ cmake .. -DCMAKE_BUILD_TYPE=Debug

# команда сборки и запуска
$ cmake --build . && bin/hello_log