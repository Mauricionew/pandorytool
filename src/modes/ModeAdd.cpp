#include <iostream>
#include "ModeAdd.h"

bool ModeAdd::validate() {
    if (!FileSystem::exists(sourceDir)) {
        std::cout << sourceDir << " does not exist " << std::endl;
        return false;
    }
    if (!FileSystem::exists(targetDir)) {
        std::cout << targetDir << " does not exist " << std::endl;
        return false;
    }
    return true;
}

ModeAdd::ModeAdd(std::string &sourceDir, std::string &targetDir) : sourceDir(sourceDir), targetDir(targetDir) {

}

int ModeAdd::main() {
    if (!validate()) {
        return 1;
    }
    std::string mc = targetDir + "/mcgames";
    std::string mcInstall = mc + "/install.txt";
    if (!FileSystem::exists(mc)) {
        std::cout << "Making " << mc << std::endl;
        bool result = FileSystem::makeDirectory(mc);
        if (!result) {
            std::cout << "Cannot create target" << std::endl;
            return 1;
        }
    }
    if (!FileSystem::exists(mcInstall)) {
        // TODO: create file
        int i =0;
    }
    return 0;
}





