cmake_minimum_required(VERSION 3.0)

add_subdirectory(drv/plic)
add_subdirectory(drv/ns16550a)
add_subdirectory(drv/virtio)

add_ramfs(plic)
add_ramfs(ns16550a)
add_ramfs(virtio)
