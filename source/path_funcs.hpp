#pragma once

#include <dirent.h>
#include <sys/stat.h>

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

// Function to create a directory if it doesn't exist
void createSingleDirectory(const std::string& directoryPath)
{
    struct stat st;
    if (stat(directoryPath.c_str(), &st) != 0) {
        mkdir(directoryPath.c_str(), 0777);
    }
}

// Function to create a directory (including nested directories) if it doesn't exist
void createDirectory(const std::string& directoryPath)
{
    std::string path = directoryPath;

    // Remove leading "sdmc:/" if present
    if (path.substr(0, 6) == "sdmc:/") {
        path = path.substr(6);
    }

    size_t pos = 0;
    std::string token;
    std::string parentPath = "sdmc:/";

    // Iterate through the path and create each directory level if it doesn't exist
    while ((pos = path.find('/')) != std::string::npos) {
        token = path.substr(0, pos);
        if (token.empty()) {
            // Skip empty tokens (e.g., consecutive slashes)
            path.erase(0, pos + 1);
            continue;
        }

        parentPath += token + "/";
        createSingleDirectory(parentPath); // Create the parent directory
        path.erase(0, pos + 1);
    }

    // Create the final directory level if it doesn't exist
    if (!path.empty()) {
        parentPath += path;
        createSingleDirectory(parentPath); // Create the final directory
    }
}

// Function to create a text file with the specified content
void createTextFile(const std::string& filePath, const std::string& content)
{
    FILE* file = std::fopen(filePath.c_str(), "w");
    if (file != nullptr) {
        std::fwrite(content.c_str(), 1, content.length(), file);
        std::fclose(file);
    }
}

void removeEntryFromList(const std::string& entry, std::vector<std::string>& fileList)
{
    fileList.erase(std::remove_if(fileList.begin(), fileList.end(), [&](const std::string& filePath) {
                       return filePath.compare(0, entry.length(), entry) == 0;
                   }),
                   fileList.end());
}

// Delete functions
bool deleteFileOrDirectory(const std::string& pathToDelete)
{
    struct stat pathStat;

    if (isDirectory(pathToDelete)) {
        FsFileSystem fsSdmc;
        if (R_FAILED(fsOpenSdCardFileSystem(&fsSdmc))) {
            log("Error accessing file system");
            return false;
        }
        if (R_FAILED(fsFsDeleteDirectoryRecursively(&fsSdmc, pathToDelete.c_str() + 5))) {
            log("Error accessing deleting the folder \"%s\"", pathToDelete.c_str() + 5);
            return false;
        }
        return true;
    } else if (stat(pathToDelete.c_str(), &pathStat) == 0) {
        if (S_ISREG(pathStat.st_mode)) {
            if (std::remove(pathToDelete.c_str()) == 0) {
                return true;
                // Deletion successful
            }
        }
    }
    return false;
}

bool deleteFileOrDirectoryByPattern(const std::string& pathPattern)
{
    //log("pathPattern: "+pathPattern);
    std::vector<std::string> fileList = getFilesListByWildcards(pathPattern);
    bool result = true;
    for (const auto& path : fileList) {
        //log("path: "+path);
        result = result && deleteFileOrDirectory(path);
        if (!result) {
            return result;
        }
    }
    return result;
}

bool mirrorDeleteFiles(const std::string& sourcePath, const std::string& targetPath = "sdmc:/")
{
    std::vector<std::string> fileList = getFilesListFromDirectory(sourcePath);
    bool result = true;
    for (const auto& path : fileList) {
        // Generate the corresponding path in the target directory by replacing the source path
        std::string updatedPath = targetPath + path.substr(sourcePath.size());
        //log("mirror-delete: "+path+" "+updatedPath);
        result = result && deleteFileOrDirectory(updatedPath);
        if (!result) {
            log("Failed to delete \"%s\"", updatedPath.c_str());
            return result;
        }
    }
    return result;
}

// Move functions
bool moveFileOrDirectory(const std::string& sourcePath, const std::string& destinationPath)
{
    struct stat sourceInfo;
    struct stat destinationInfo;

    //log("sourcePath: "+sourcePath);
    //log("destinationPath: "+destinationPath);

    if (stat(sourcePath.c_str(), &sourceInfo) == 0) {
        // Source file or directory exists

        // Check if the destination path exists
        bool destinationExists = (stat(getParentDirFromPath(destinationPath).c_str(), &destinationInfo) == 0);
        if (!destinationExists) {
            // Create the destination directory
            createDirectory(getParentDirFromPath(destinationPath));
        }

        if (S_ISDIR(sourceInfo.st_mode)) {
            // Source path is a directory
            DIR* dir = opendir(sourcePath.c_str());
            if (!dir) {
                //log("Failed to open source directory: "+sourcePath);
                //printf("Failed to open source directory: %s\n", sourcePath.c_str());
                return false;
            }

            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                const std::string fileOrFolderName = entry->d_name;

                if (fileOrFolderName != "." && fileOrFolderName != "..") {
                    std::string sourceFilePath = sourcePath + fileOrFolderName;
                    std::string destinationFilePath = destinationPath + fileOrFolderName;

                    if (entry->d_type == DT_DIR) {
                        // Append trailing slash to destination path for folders
                        destinationFilePath += "/";
                        sourceFilePath += "/";
                    }

                    moveFileOrDirectory(sourceFilePath, destinationFilePath);
                }
            }

            closedir(dir);

            // Delete the source directory
            deleteFileOrDirectory(sourcePath);

            return true;
        } else {
            // Source path is a regular file
            std::string filename = getNameFromPath(sourcePath);

            std::string destinationFilePath = destinationPath;

            if (destinationPath[destinationPath.length() - 1] == '/') {
                destinationFilePath += filename;
            }

            //log("sourcePath: "+sourcePath);
            //log("destinationFilePath: "+destinationFilePath);

            deleteFileOrDirectory(destinationFilePath); // delete destiantion file for overwriting
            if (rename(sourcePath.c_str(), destinationFilePath.c_str()) == -1) {
                //printf("Failed to move file: %s\n", sourcePath.c_str());
                //log("Failed to move file: "+sourcePath);
                return false;
            }

            return true;
        }
    }

    // Move unsuccessful or source file/directory doesn't exist
    return false;
}

bool moveFilesOrDirectoriesByPattern(const std::string& sourcePathPattern, const std::string& destinationPath)
{
    std::vector<std::string> fileList = getFilesListByWildcards(sourcePathPattern);
    bool result = true;
    std::string fileListAsString;
    for (const std::string& filePath : fileList) {
        fileListAsString += filePath + "\n";
    }
    //log("File List:\n" + fileListAsString);

    //log("pre loop");
    // Iterate through the file list
    for (const std::string& sourceFileOrDirectory : fileList) {
        //log("sourceFileOrDirectory: "+sourceFileOrDirectory);
        // if sourceFile is a file (Needs condition handling)
        if (!isDirectory(sourceFileOrDirectory)) {
            //log("destinationPath: "+destinationPath);
            result = result && moveFileOrDirectory(sourceFileOrDirectory, destinationPath);
            if (!result) {
                return result;
            }
        } else if (isDirectory(sourceFileOrDirectory)) {
            // if sourceFile is a directory (needs conditoin handling)
            std::string folderName = getNameFromPath(sourceFileOrDirectory);
            std::string fixedDestinationPath = destinationPath + folderName + "/";

            //log("fixedDestinationPath: "+fixedDestinationPath);

            result = result && moveFileOrDirectory(sourceFileOrDirectory, fixedDestinationPath);
            if (!result) {
                return result;
            }
        }
    }
    return result;
    //log("post loop");
}

// Copy functions
bool copySingleFile(const std::string& fromFile, const std::string& toFile)
{
    FILE* srcFile = fopen(fromFile.c_str(), "rb");
    FILE* destFile = fopen(toFile.c_str(), "wb");
    if (srcFile && destFile) {
        const size_t bufferSize = 8192;
        char buffer[bufferSize];
        size_t bytesRead;

        while ((bytesRead = fread(buffer, 1, bufferSize, srcFile)) > 0) {
            fwrite(buffer, 1, bytesRead, destFile);
        }

        fclose(srcFile);
        fclose(destFile);
    } else {
        return false;
        // Error opening files or performing copy action.
        // Handle the error accordingly.
    }
    return true;
}

bool copyFileOrDirectory(const std::string& fromFileOrDirectory, const std::string& toFileOrDirectory)
{
    bool result = true;
    struct stat fromFileOrDirectoryInfo;
    if (stat(fromFileOrDirectory.c_str(), &fromFileOrDirectoryInfo) == 0) {
        if (S_ISREG(fromFileOrDirectoryInfo.st_mode)) {
            // Source is a regular file
            std::string fromFile = fromFileOrDirectory;

            struct stat toFileOrDirectoryInfo;

            if (stat(toFileOrDirectory.c_str(), &toFileOrDirectoryInfo) == 0 && S_ISDIR(toFileOrDirectoryInfo.st_mode)) {
                // Destination is a directory
                std::string toDirectory = toFileOrDirectory;
                std::string fileName = fromFile.substr(fromFile.find_last_of('/') + 1);
                std::string toFilePath = toDirectory + fileName;

                // Create the destination directory if it doesn't exist
                createDirectory(toDirectory);

                // Check if the destination file exists and remove it
                if (stat(toFilePath.c_str(), &toFileOrDirectoryInfo) == 0 && S_ISREG(toFileOrDirectoryInfo.st_mode)) {
                    std::remove(toFilePath.c_str());
                }

                return copySingleFile(fromFile, toFilePath);
            } else {
                std::string toFile = toFileOrDirectory;
                // Destination is a file or doesn't exist
                std::string toDirectory = toFile.substr(0, toFile.find_last_of('/'));

                // Create the destination directory if it doesn't exist
                createDirectory(toDirectory);

                // Destination is a file or doesn't exist
                // Check if the destination file exists and remove it
                if (stat(toFile.c_str(), &toFileOrDirectoryInfo) == 0 && S_ISREG(toFileOrDirectoryInfo.st_mode)) {
                    std::remove(toFile.c_str());
                }

                return copySingleFile(fromFile, toFile);
            }
        } else if (S_ISDIR(fromFileOrDirectoryInfo.st_mode)) {
            // Source is a directory
            std::string fromDirectory = fromFileOrDirectory;
            //log("fromDirectory: "+fromDirectory);

            struct stat toFileOrDirectoryInfo;
            if (stat(toFileOrDirectory.c_str(), &toFileOrDirectoryInfo) == 0 && S_ISDIR(toFileOrDirectoryInfo.st_mode)) {
                // Destination is a directory
                std::string toDirectory = toFileOrDirectory;
                std::string dirName = getNameFromPath(fromDirectory);
                if (dirName != "") {
                    std::string toDirPath = toDirectory + dirName + "/";
                    //log("toDirectory: "+toDirectory);
                    //log("dirName: "+dirName);
                    //log("toDirPath: "+toDirPath);

                    // Create the destination directory
                    createDirectory(toDirPath);
                    //mkdir(toDirPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

                    // Open the source directory
                    DIR* dir = opendir(fromDirectory.c_str());
                    if (dir != nullptr) {
                        dirent* entry;
                        while ((entry = readdir(dir)) != nullptr) {
                            std::string fileOrFolderName = entry->d_name;
                            // handle cade for files
                            if (fileOrFolderName != "." && fileOrFolderName != "..") {
                                std::string fromFilePath = fromDirectory + fileOrFolderName;
                                result = result && copyFileOrDirectory(fromFilePath, toDirPath);
                                if (!result) {
                                    closedir(dir);
                                    return result;
                                }
                            }
                            // handle case for subfolders within the from file path
                            if (entry->d_type == DT_DIR && fileOrFolderName != "." && fileOrFolderName != "..") {
                                std::string subFolderPath = fromDirectory + fileOrFolderName + "/";
                                result = result && copyFileOrDirectory(subFolderPath, toDirPath);
                                if (!result) {
                                    closedir(dir);
                                    return result;
                                }
                            }
                        }
                        closedir(dir);
                    }
                }
            }
        }
    }
    return result;
}

bool copyFileOrDirectoryByPattern(const std::string& sourcePathPattern, const std::string& toDirectory)
{
    std::vector<std::string> fileList = getFilesListByWildcards(sourcePathPattern);
    bool result = true;

    for (const std::string& sourcePath : fileList) {
        //log("sourcePath: "+sourcePath);
        //log("toDirectory: "+toDirectory);
        if (sourcePath != toDirectory) {
            result = result && copyFileOrDirectory(sourcePath, toDirectory);
            if (!result) {
                return result;
            }
        } else {
            return false;
        }
    }
    return result;
}

bool mirrorCopyFiles(const std::string& sourcePath, const std::string& targetPath = "sdmc:/")
{
    std::vector<std::string> fileList = getFilesListFromDirectory(sourcePath);
    bool result = true;

    for (const auto& path : fileList) {
        // Generate the corresponding path in the target directory by replacing the source path
        std::string updatedPath = targetPath + path.substr(sourcePath.size());
        if (path != updatedPath) {
            //log("mirror-copy: "+path+" "+updatedPath);
            result = result && copyFileOrDirectory(path, updatedPath);
            if (!result) {
                return result;
            }
        } else {
            return false;
        }
    }
    return result;
}

bool generateBackup()
{
    int highestNumber = 0;
    std::regex pattern(R"(Backup \[(\d+)\])");
    namespace fs = std::filesystem;
    if (!isDirectory("/atmosphere/kips/.bak/"))
        createDirectory("/atmosphere/kips/.bak/");

    for (const auto& entry : fs::directory_iterator("/atmosphere/kips/.bak/")) {
        if (fs::is_regular_file(entry)) {
            std::smatch match;
            std::string filename = entry.path().filename().string();
            if (std::regex_search(filename, match, pattern)) {
                int number = std::stoi(match[1].str());
                highestNumber = std::max(highestNumber, number);
            }
        }
    }

    std::string backupName = "/atmosphere/kips/.bak/Backup [" + std::to_string(highestNumber + 1) + "].kip";
    // Save current kip with the unique name
    bool result = copyFileOrDirectory("/atmosphere/kips/loader.kip", backupName);
    return result;
	}

void saveKipBackup2Json(const std::string backupContent) {
    int highestNumber = 0;
    namespace fs = std::filesystem;
    std::regex pattern(R"(Backup \[(\d+)\])");

    const std::string kipBackupJsonPath = "/atmosphere/kips/kip-json/";
    if (!isDirectory(kipBackupJsonPath))
        createDirectory(kipBackupJsonPath);

    for (const auto& entry : fs::directory_iterator(kipBackupJsonPath)) {
        if (fs::is_regular_file(entry)) {
            std::smatch match;
            std::string filename = entry.path().filename().string();
            if (std::regex_search(filename, match, pattern)) {
                int number = std::stoi(match[1].str());
                highestNumber = std::max(highestNumber, number);
            }
        }
    }
    std::string backupFilePath = kipBackupJsonPath + "Backup [" + std::to_string(highestNumber + 1) + "].json";
    
    createTextFile(backupFilePath, backupContent);
}