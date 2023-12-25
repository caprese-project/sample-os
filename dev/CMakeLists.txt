cmake_minimum_required(VERSION 3.0)

add_subdirectory(plic)
add_subdirectory(ns16550a)
add_subdirectory(virtio)

add_ramfs(plic)
add_ramfs(ns16550a)
add_ramfs(virtio)
