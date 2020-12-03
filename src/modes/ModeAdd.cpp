#include <iostream>
#include <filesystem>
#include <algorithm>
#include <tinyxml2.h>
#include "ModeAdd.h"
#include "../McGamesTXT.h"
#include "../termcolor/termcolor.hpp"
#include "../EditionCheck.h"

std::string ModeAdd::pad(std::string string, const size_t size, const char character = ' ') {
    if (size > string.size()) {
        string.insert(0, size - string.size(), character);
    }
    return string;
}

bool ModeAdd::validate() {
    if (!Fs::exists(sourceDir)) {
        std::cout << sourceDir << " does not exist " << std::endl;
        return false;
    }
    if (!Fs::exists(targetDir)) {
        std::cout << targetDir << " does not exist " << std::endl;
        return false;
    }
    return true;
}

ModeAdd::ModeAdd(std::string &sourceDir, std::string &targetDir) : sourceDir(sourceDir), targetDir(targetDir) {
}

void ModeAdd::getScreenScraperDetails() {
    std::cout << "To add games, you need to have a free ScreenScraper.fr account." << std::endl;
    std::cout << "Register one here: https://screenscraper.fr/membreinscription.php" << std::endl << std::endl;
    std::cout << "Please enter your screenscraper.fr username: ";
    std::cin >> screenscrapeUsername;
    scrapeService.setUsername(screenscrapeUsername);
    std::cout << "Please enter your screenscraper.fr password: ";
    console.disableEcho();
    std::cin >> screenscrapePassword;
    scrapeService.setPassword(screenscrapeUsername);
    console.enableEcho();
}

// 1. Add games (MCGames)
int ModeAdd::main() {
    if (!validate()) {
        return 1;
    }
    // getScreenScraperDetails();
    if (screenscrapeUsername.empty() || screenscrapePassword.empty()) {
        return 1;
    }
    if (!Fs::exists("scrapes")) {
        Fs::makeDirectory("scrapes");
    }
    createTargetDirectory();
    resetInstallFile();
    resetMcGamesFolder();

    parseSourceDirectory();
    return 0;
}

void ModeAdd::parseSourceDirectory() {
    openInstallFileHandle();
    for (const auto &entry : std::filesystem::directory_iterator(this->sourceDir)) {
        std::string filePath = entry.path().string();
        std::string basename = Fs::basename(filePath);
        parseRomFolder(filePath);
    }
    closeInstallFileHandle();
}

void ModeAdd::parseRomFolder(const std::string& romFolder) {
    for (const auto &entry : std::filesystem::directory_iterator(romFolder)) {
        std::string filePath = entry.path().string();
        std::cout << "Processing: " << filePath << " ";

        scrapeService.setFilename(filePath);
        int result = scrapeService.scrapeRom();
        if (result == 0) {
            processRom();
        }
    }
}

void ModeAdd::processRom()
{
    scrapeService.convertXML();
}



void ModeAdd::openInstallFileHandle() {
    installFile.open(getInstallFilePath().c_str());
}

void ModeAdd::closeInstallFileHandle() {
    installFile.close();
}

void ModeAdd::parseSourceGameXML(const std::string &gameListXml) {
    tinyxml2::XMLDocument doc;
    FILE * xmlFile;
    xmlFile = fopen(gameListXml.c_str(), "rb");
    doc.LoadFile(xmlFile);
    std::string directory = Fs::dirname(gameListXml);
    tinyxml2::XMLElement *gameList = doc.FirstChildElement("gameList");
    tinyxml2::XMLElement *provider = gameList->FirstChildElement("provider");
    std::string system = Fs::basename(directory);
    int i = 1;

    for (tinyxml2::XMLElement *game = gameList->FirstChildElement("game");
         game != nullptr;
         game = game->NextSiblingElement("game")) {
        const char *romPath = game->FirstChildElement("path")->GetText();
        const char *romName = game->FirstChildElement("name")->GetText();
        std::string absoluteRomPath = directory + "/" + romPath;

#ifndef NO_SHAREWARE_LIMIT
        int limit = 20;
        if (i > limit) {
            std::cout << std::endl;
            std::cout << "pandorytool is freeware and is the product of many hours of blood, sweat and tears. " << std::endl;
            std::cout << "This version is limited to " << limit << " roms." << std::endl;
            std::cout << "If you wish to transfer more, you can compile the source ";
            std::cout << "code yourself, or consider supporting us by grabbing us a coffee at" << std::endl;
            std::cout << "https://www.buymeacoffee.com/CKZbiXa and we will send you a copy of the unlimited version. Thanks!";
            std::cout << std::endl;
            break;
        }
#endif

        if (!Fs::exists(absoluteRomPath)) {
            continue;
        }


        std::string shortSystemName = systemMapper.convertDirectoryNameToSystemName(system);
        if (!shortSystemName.empty()) {
            std::string targetRomName = shortSystemName + pad(std::to_string(i), 4, '0');
            EditionCheck ed;
            if (ed.isUltimate()) {
                // less annoying file names
                tinyxml2::XMLElement *hashElement = game->FirstChildElement("hash");
                if (hashElement != nullptr) {
                    const char *hash = hashElement->GetText();
                    if (hash != nullptr) {
                        targetRomName = shortSystemName + hash;
                    }
                }
            }
            if (!systemMapper.getSystemRenameFlag(system)) {
                targetRomName = Fs::stem(romPath);
            }

            std::string targetRomDir = targetDir + "/mcgames/" + targetRomName;
            std::string systemName = extractXMLText(provider->FirstChildElement("System"));
            std::cout << "- Found ";
            systemMapper.setConsoleColourBySystem(system);
            std::cout << systemName;
            std::cout << termcolor::reset;
            std::cout << " ROM: " << romName << " [ " << Fs::basename(romPath);
            std::cout << " ]" << std::endl;

            if (!Fs::exists(targetRomDir)) {
                Fs::makeDirectory(targetRomDir);
            }
            try {
                bool rename = systemMapper.getSystemRenameFlag(system);
                copyRomToDestination(absoluteRomPath, targetRomDir, rename);
            } catch (...) {
                std::cout << " ## ERROR COPYING: " << absoluteRomPath << " TO " << targetRomName << "...skipping" << std::endl;
                continue;
            }

            std::string videoPath;
            if (game->FirstChildElement("video") != nullptr) {
                videoPath = extractXMLText(game->FirstChildElement("video"));
                std::string absoluteVideoPath = directory;
                absoluteVideoPath += "/" + videoPath;
                if (Fs::exists(absoluteVideoPath) && !videoPath.empty()) {
                    copyRomVideoToDestination(absoluteVideoPath, targetRomDir);
                }
            }
            installFile << targetRomName << std::endl;

            // mame controls
            std::string additionalRom;
            std::string controlsPath = "./controls/" + system + "/" + targetRomName + ".cfg";

            if (Fs::exists(controlsPath)) {
                additionalRom = Fs::basename(controlsPath);
                copyRomToDestination(controlsPath, targetRomDir, false);
            }

            generateMcGamesMeta(game, system, shortSystemName, targetRomDir, targetRomName, additionalRom);
            i++;
        } else {
            std::cout << "Unknown system detected in source folder: " << system << std::endl;
        }
    }
    fclose(xmlFile);
}

// Checks subfolders within “source rom folder”.
// Copies both rom file
// – to usbdevice:\mcgames\DC0001 (if dreamcast game) – check “ARSENAME” below for naming convention.
// Both DC0001.cdi should be within the mcgames/DC0001 folder.
void ModeAdd::copyRomToDestination(const std::string &rom, const std::string &destination, bool rename) {
    std::string basename;
    std::string extension;
    if (rename) {
        basename = Fs::basename(destination);
        extension = Fs::extension(rom);
    } else {
        basename = Fs::basename(rom);
        extension = "";
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    Fs::makeDirectory(destination);
    Fs::copy(rom, destination + "/" + basename + extension);
}

// – (and if available, mp4 file (from romsubfolder/media/videos))
// DC0001.mp4 should be within the mcgames/DC0001 folder.
void ModeAdd::copyRomVideoToDestination(const std::string &absoluteVideoPath, const std::string &destination) {
    std::string basename = Fs::basename(destination);
    std::string extension = Fs::extension(absoluteVideoPath);
    Fs::makeDirectory(destination);
    Fs::copy(absoluteVideoPath, destination + "/" + basename + extension);
}

// remove install.txt file if exists, open clear file
void ModeAdd::resetInstallFile() {
    std::string mcInstall = getInstallFilePath();
    FILE *foo;
    foo = fopen(mcInstall.c_str(), "w");
    fclose(foo);
}

void ModeAdd::resetMcGamesFolder() {
    std::string mgGamesFolder = getMcGamesFolder();
    Fs::remove(getMcGamesFolder());
    Fs::makeDirectory(getMcGamesFolder());
}

std::string ModeAdd::getInstallFilePath() {
    std::string mcInstall = this->targetDir + "/mcgames/install.txt";
    return mcInstall;
}

bool ModeAdd::createTargetDirectory() {
    std::string mc = getMcGamesFolder();
    bool result = false;
    if (!Fs::exists(mc)) {
        std::cout << "Making " << mc << std::endl;
        result = Fs::makeDirectory(mc);
        if (!result) {
            std::cout << "Cannot create target" << std::endl;
        }
    }
    return result;
}

std::string ModeAdd::getMcGamesFolder() {
    std::string mc = this->targetDir + "/mcgames";
    return mc;
}

std::string ModeAdd::extractXMLText(tinyxml2::XMLElement *elem) {
    if (elem != nullptr) {
        if (elem->GetText() != nullptr) {
            return elem->GetText();
        }
    }
    return std::string();
}


// Template.txt and template.xml (from template file) should be then copied to the DC0001 folder with the
// following variables changed depending on system and game (see below)
// install.txt file should then be appended with the “ARSENAME” (DC0001)
// repeat / loop process until all roms have been added.
void ModeAdd::generateMcGamesMeta(tinyxml2::XMLElement *sourceGame, std::string system, std::string shortSystemName, std::string romPath,
                                  std::string targetRomName, std::string additionalRom) {

    // emulator type check code definitely bullshit- cant get this if statement to work ;(
    int emutype = 99;
    int emuload = 99;
    //test cout << "short system name is " << shortSystemName << endl;
    if (shortSystemName == "FBA") {
        emutype = 1;
        emuload = 2;
    }
    if (shortSystemName == "MAME37") {
        emutype = 2;
        emuload = 0;
    }
    if (shortSystemName == "MAME139") {
        emutype = 3;
        emuload = 0;
    }
    if (shortSystemName == "MAME78") {
        emutype = 4;
        emuload = 0;
    }
    if (shortSystemName == "PSP") {
        emutype = 6;
        emuload = 3;
    }
    if (shortSystemName == "PS") {
        emutype = 7;
        emuload = 0;
    }
    if (shortSystemName == "N64") {
        emutype = 8;
        emuload = 3;
    }
    if (shortSystemName == "NES") {
        emutype = 11;
        emuload = 3;
    }
    if (shortSystemName == "SNES") {
        emutype = 12;
        emuload = 0;
    }
    if (shortSystemName == "GBA") {
        emutype = 13;
        emuload = 0;
    }
     if (shortSystemName == "GB") {
        emutype = 14;
        emuload = 0;
    }
    if (shortSystemName == "GBC") {
        emutype = 14;
        emuload = 0;
    }
    if (shortSystemName == "MD") {
        emutype = 15;
        emuload = 3;
    }
    if (shortSystemName == "WSWAN") {
        emutype = 16;
        emuload = 0;
    }
    if (shortSystemName == "PCE") {
        emutype = 17;
        emuload = 0;
    }
    if (shortSystemName == "DC") {
        emutype = 18;
        emuload = 3;
    }
    if (shortSystemName == "MAME19") {
        emutype = 19;
        emuload = 0;
    }
    if (shortSystemName == "MS") {
        emutype = 15;
        emuload = 3;
    }
    if (shortSystemName == "GG") {
        emutype = 15;
        emuload = 3;
    }
    if (shortSystemName == "32X") {
        emutype = 15;
        emuload = 3;
}
    std::string emuString = std::to_string(emutype);
    std::string emuStringload = std::to_string(emuload);

    std::string name = extractXMLText(sourceGame->FirstChildElement("name"));
    std::string desc = extractXMLText(sourceGame->FirstChildElement("desc"));
    std::string relativeRomPath = Fs::basename(extractXMLText(sourceGame->FirstChildElement("path")));
    std::string dateString = extractXMLText(sourceGame->FirstChildElement("releasedate"));
    int year = (!dateString.empty()) ? std::stoi(dateString.substr(0, 4)) : 0;
  //  std::string players = "1";
    std::string players = extractXMLText(sourceGame->FirstChildElement("players"));
    std::string developer = extractXMLText(sourceGame->FirstChildElement("developer"));
    std::string genre = extractXMLText(sourceGame->FirstChildElement("genre"));
    std::string targetXMLFile = romPath + "/" + targetRomName + ".xml";
    std::string targetTXTFile = romPath + "/" + targetRomName + ".txt";

    bool rename = systemMapper.getSystemRenameFlag(system);

    std::string romFileName = relativeRomPath;
    if (rename) {
        romFileName = targetRomName + Fs::extension(relativeRomPath);
    } else {
        romFileName = relativeRomPath;
    }

    std::string romFileNameBase = Fs::stem(romFileName);
    std::string romFileNameExt = Fs::extension(romFileName);
    std::string romFolder = Fs::dirname(romFileName);
    std::transform(romFileNameExt.begin(), romFileNameExt.end(), romFileNameExt.begin(), ::tolower);

    romFileName = romFolder + romFileNameBase + romFileNameExt;

    McGamesXML mcXML;
    mcXML.setEmulatorId(emuString);
    mcXML.setEmulatorLoad(emuStringload);
    mcXML.setRomTitle(name);
    mcXML.setRomFileName(romFileName);
    mcXML.setRomShortId(targetRomName);
    mcXML.setPlayers(players);
    mcXML.setRomDescription(desc);
    mcXML.setLanguage("EN"); //TODO is this always true?
    mcXML.setYear(year);
    mcXML.setGenre(systemMapper.getGenre(genre));
    mcXML.setRomDeveloper(developer);
    mcXML.setRomPath(relativeRomPath);
    mcXML.setSaveState(systemMapper.getSystemSaveState(system));

    if (!additionalRom.empty()) {
        mcXML.addAdditionalRom(additionalRom);
    }

    mcXML.generate(targetXMLFile);

    McGamesTXT mcTXT;
    mcTXT.setEmulatorId(emuString);
    mcTXT.setEmulatorLoad(emuStringload);
    mcTXT.setRomTitle(name);
    mcTXT.setRomShortId(targetRomName);
    mcTXT.setRomDescription(desc);
    mcTXT.setLanguage("EN"); //TODO is this always true?
    mcTXT.setYear(year);
    mcTXT.setGenre(systemMapper.getGenre(genre));
    mcTXT.setRomDeveloper(developer);
    mcTXT.setRomPath(relativeRomPath);
    mcTXT.generate(targetTXTFile);
}

