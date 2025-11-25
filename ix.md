# Instant eXecutable (IX) File format

struct Function {
    char* name;
    uint64_t locationInFile;
}

struct DynamicFile {
    uint64_t size;
    uint64_t locSize;
    char* location;

    uint64_t functions;
    Function* functionList;
}

struct IX {
    uint64_t appNameSize;
    char* appName;
    uint64_t dynamicFiles;
    DynamicFile* files;
    uint64_t entryPoint;

    char* code; // .data, .text .bss etc go here
}