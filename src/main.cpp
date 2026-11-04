#include <iostream>
#include <string>

#include "Commands.h"
#include "KvStore.h"
#include "Tokenizer.h"

int main() {
    sparkDB::KvStore store;
    std::string line;

    while (std::getline(std::cin, line)) {
        if (line == "exit") {
            break;
        }

        const auto tokens = sparkDB::Tokenize(line);
        if (!tokens) {
            std::cout << "(error) ERR invalid quoting\n";
        } else if (!tokens->empty()) {
            std::cout << sparkDB::ToDisplay(sparkDB::Execute(store, *tokens)) << '\n';
        }
    }

    return 0;
}
