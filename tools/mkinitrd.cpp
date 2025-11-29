#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <filesystem>

constexpr uint32_t INITRD_MAGIC = 0x44524E49;

struct InitrdFile {
    char name[64];
    uint64_t offset;
    uint64_t size;
};

struct InitrdHeader {
    uint32_t magic;
    uint32_t fileCount;
};

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: mkinitrd <output.img> <file1> [file2] ..." << std::endl;
        return 1;
    }
    
    std::string outputPath = argv[1];
    std::vector<std::string> inputFiles;
    
    for (int i = 2; i < argc; i++) {
        inputFiles.push_back(argv[i]);
    }
    
    std::vector<InitrdFile> files;
    std::vector<std::vector<uint8_t>> fileData;
    
    uint64_t currentOffset = sizeof(InitrdHeader) + inputFiles.size() * sizeof(InitrdFile);
    
    for (const auto& path : inputFiles) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            std::cerr << "Failed to open: " << path << std::endl;
            return 1;
        }
        
        size_t size = file.tellg();
        file.seekg(0);
        
        std::vector<uint8_t> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        
        InitrdFile fileEntry;
        std::memset(&fileEntry, 0, sizeof(fileEntry));
        
        std::filesystem::path p(path);
        std::string filename = p.filename().string();
        std::strncpy(fileEntry.name, filename.c_str(), 63);
        fileEntry.offset = currentOffset;
        fileEntry.size = size;
        
        files.push_back(fileEntry);
        fileData.push_back(data);
        
        currentOffset += size;
        
        std::cout << "Added: " << filename << " (" << size << " bytes)" << std::endl;
    }
    
    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        std::cerr << "Failed to create output file" << std::endl;
        return 1;
    }
    
    InitrdHeader header;
    header.magic = INITRD_MAGIC;
    header.fileCount = files.size();
    
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    for (const auto& file : files) {
        output.write(reinterpret_cast<const char*>(&file), sizeof(file));
    }
    
    for (const auto& data : fileData) {
        output.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
    
    std::cout << "Created initrd: " << outputPath << std::endl;
    std::cout << "  Files: " << files.size() << std::endl;
    std::cout << "  Total size: " << currentOffset << " bytes" << std::endl;
    
    return 0;
}
