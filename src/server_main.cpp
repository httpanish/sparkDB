#include <iostream>

#include "KvStore.h"
#include "Server.h"

int main() {
    sparkDB::KvStore store;
    sparkDB::Server server(store);

    if (!server.Listen(6379)) {
        std::cerr << "failed to listen on 127.0.0.1:6379\n";
        return 1;
    }

    server.Run();
    return 0;
}
