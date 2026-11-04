#ifndef SPARKDB_COMMANDS_H
#define SPARKDB_COMMANDS_H

#include <string>
#include <vector>

#include "KvStore.h"

namespace sparkDB {
    // Runs a command and returns the text to show. args[0] is the command name,
    // in any case. args must not be empty.
    std::string Execute(KvStore &store, const std::vector<std::string> &args);
}

#endif //SPARKDB_COMMANDS_H
