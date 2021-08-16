cmake_minimum_required(VERSION 3.4.1)

# Compiler options
add_compile_options(-Wall)

# Submodule library
add_library(bfs
            STATIC

            # Source
            bfs_file.c
            bfs_util.c)

# Linking
target_link_libraries(bfs

                      # NDK libraries
                      log)
