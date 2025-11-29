cmake_minimum_required(VERSION 3.30)
project(Coursework_Petryk_PO)

set(CMAKE_CXX_STANDARD 20)

set(SERVER_SOURCES
        server/src/main.cpp
        server/src/Task.cpp
        server/src/ThreadPool.cpp
        server/src/Queue.cpp
        server/src/SearchResult.cpp
        server/src/FileManager.cpp
        server/src/InvertedIndex.cpp
        server/src/Server.cpp
)

set(SERVER_HEADERS
        server/include/Task.h
        server/include/ThreadPool.h
        server/include/Queue.h
        server/include/SearchResult.h
        server/include/FileManager.h
        server/include/InvertedIndex.h
        server/include/Server.h
)

set(CLIENT_SOURCES
        client/src/main.cpp
        client/src/Client.cpp
)
set(CLIENT_HEADERS
        client/include/Client.h
)

add_executable(Server ${SERVER_SOURCES} ${SERVER_HEADERS})
add_executable(Client ${CLIENT_SOURCES} ${CLIENT_HEADERS})

target_include_directories(Server PUBLIC server/include)
target_include_directories(Client PUBLIC client/include)