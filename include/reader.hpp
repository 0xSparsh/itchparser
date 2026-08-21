#pragma once

class MMapFile
{
public:
    explicit MMapFile(const char* FileName) noexcept;
    
    ~MMapFile();

    MMapFile(const MMapFile& other) = delete;

    MMapFile& operator=(const MMapFile& other) = delete;

    MMapFile(MMapFile&& other);

    MMapFile& operator=(MMapFile&& other);

private:

};