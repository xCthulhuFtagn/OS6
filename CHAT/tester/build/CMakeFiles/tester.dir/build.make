# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.18

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Disable VCS-based implicit rules.
% : %,v


# Disable VCS-based implicit rules.
% : RCS/%


# Disable VCS-based implicit rules.
% : RCS/%,v


# Disable VCS-based implicit rules.
% : SCCS/s.%


# Disable VCS-based implicit rules.
% : s.%


.SUFFIXES: .hpux_make_needs_suffix_list


# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/build

# Include any dependencies generated for this target.
include CMakeFiles/tester.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/tester.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/tester.dir/flags.make

CMakeFiles/tester.dir/main.cpp.o: CMakeFiles/tester.dir/flags.make
CMakeFiles/tester.dir/main.cpp.o: ../main.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/tester.dir/main.cpp.o"
	/usr/bin/g++-11 $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/tester.dir/main.cpp.o -c /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/main.cpp

CMakeFiles/tester.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tester.dir/main.cpp.i"
	/usr/bin/g++-11 $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/main.cpp > CMakeFiles/tester.dir/main.cpp.i

CMakeFiles/tester.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tester.dir/main.cpp.s"
	/usr/bin/g++-11 $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/main.cpp -o CMakeFiles/tester.dir/main.cpp.s

CMakeFiles/tester.dir/logger.cpp.o: CMakeFiles/tester.dir/flags.make
CMakeFiles/tester.dir/logger.cpp.o: ../logger.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/tester.dir/logger.cpp.o"
	/usr/bin/g++-11 $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/tester.dir/logger.cpp.o -c /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/logger.cpp

CMakeFiles/tester.dir/logger.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/tester.dir/logger.cpp.i"
	/usr/bin/g++-11 $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/logger.cpp > CMakeFiles/tester.dir/logger.cpp.i

CMakeFiles/tester.dir/logger.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/tester.dir/logger.cpp.s"
	/usr/bin/g++-11 $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/logger.cpp -o CMakeFiles/tester.dir/logger.cpp.s

# Object files for target tester
tester_OBJECTS = \
"CMakeFiles/tester.dir/main.cpp.o" \
"CMakeFiles/tester.dir/logger.cpp.o"

# External object files for target tester
tester_EXTERNAL_OBJECTS =

bin/tester: CMakeFiles/tester.dir/main.cpp.o
bin/tester: CMakeFiles/tester.dir/logger.cpp.o
bin/tester: CMakeFiles/tester.dir/build.make
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_contract.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_coroutine.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_fiber_numa.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_fiber.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_context.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_graph.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_iostreams.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_json.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_locale.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_log_setup.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_log.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_math_c99.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_math_c99f.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_math_c99l.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_math_tr1.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_math_tr1f.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_math_tr1l.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_nowide.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_program_options.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_random.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_regex.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_stacktrace_addr2line.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_stacktrace_backtrace.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_stacktrace_basic.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_stacktrace_noop.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_timer.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_type_erasure.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_thread.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_chrono.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_container.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_date_time.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_unit_test_framework.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_prg_exec_monitor.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_test_exec_monitor.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_exception.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_wave.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_filesystem.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_atomic.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_wserialization.a
bin/tester: /home/owner/.conan/data/boost/1.78.0/_/_/package/8cc3305c27e5ff838d1c7590662e309638310dfc/lib/libboost_serialization.a
bin/tester: /home/owner/.conan/data/zlib/1.2.12/_/_/package/be27726f9885116da1158027505be62e913cd585/lib/libz.a
bin/tester: /home/owner/.conan/data/bzip2/1.0.8/_/_/package/76bd63d0cd275bc555bda09b7f93740254ba3515/lib/libbz2.a
bin/tester: /home/owner/.conan/data/libbacktrace/cci.20210118/_/_/package/be27726f9885116da1158027505be62e913cd585/lib/libbacktrace.a
bin/tester: CMakeFiles/tester.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Linking CXX executable bin/tester"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/tester.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/tester.dir/build: bin/tester

.PHONY : CMakeFiles/tester.dir/build

CMakeFiles/tester.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/tester.dir/cmake_clean.cmake
.PHONY : CMakeFiles/tester.dir/clean

CMakeFiles/tester.dir/depend:
	cd /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/build /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/build /home/owner/Desktop/dev/cpp/programming/OS6/CHAT/tester/build/CMakeFiles/tester.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/tester.dir/depend

