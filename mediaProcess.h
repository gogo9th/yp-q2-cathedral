#pragma once

#include <memory>
#include <vector>
#include <filesystem>
#include <functional>

#include "filter.h"
#include "FilterFabric.hpp"


struct FileItem
{
    std::filesystem::path input;
    std::filesystem::path output;
    bool normalize;
};


class MediaProcess
{
    MediaProcess(const MediaProcess &) = delete;
    MediaProcess operator=(const MediaProcess &) = delete;
public:
    MediaProcess(const FilterFabric & fab)
        : filterFab_(fab)
    {}

    #if defined(_WIN32)
    std::wstring
    #else
    std::string
    #endif
        operator()(const FileItem & item) const;

private:
    void process(const FileItem & item) const;
    bool do_process(const FileItem & item, std::vector<float> & normalizers, bool & ripped) const;

    FilterFabric filterFab_;
};
