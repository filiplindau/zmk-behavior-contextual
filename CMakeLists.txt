#target_sources(app PRIVATE src/behavior_contextual.c)
# Initialize this directory as a Zephyr library
zephyr_library()

# Add your C source file to the library. 
# We use ../ because this CMake file is executing inside the zephyr/ subdirectory.
zephyr_library_sources(src/behavior_contextual.c)
zephyr_include_directories(${APPLICATION_SOURCE_DIR}/include)
