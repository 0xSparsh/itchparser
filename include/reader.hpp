#pragma once

class MMapFile
{
public:
    explicit MMapFile(const char* FileName) noexcept;
    
    ~MMapFile();

    MMapFile(const MMapFile& other);

    MMapFile& operator=(const MMapFile& other);

    MMapFile(MMapFile&& other);

    MMapFile& operator=(MMapFile&& other);

private:

};