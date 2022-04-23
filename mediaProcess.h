#pragma once

#include <memory>
#include <vector>
#include <filesystem>
#include <functional>

#include "filter.h"


struct FileItem
{
    std::filesystem::path input;
    std::filesystem::path output;
};

class FilterFab
{
public:
    // 'ch,10,9'
    bool addDesc(const std::string & desc);
    std::vector<std::unique_ptr<Filter>> create(int sampleRate) const;

private:
    std::vector<std::function<std::unique_ptr<Filter>(int)>> filterCtors_;
};


class MediaProcess
{
    MediaProcess(const MediaProcess &) = delete;
    MediaProcess operator=(const MediaProcess &) = delete;
public:
    MediaProcess(const FilterFab & fab)
        : filterFab_(fab)
    {}

    std::string operator()(const FileItem & item) const;

private:
    void process(const FileItem & item) const;

    const FilterFab   filterFab_;
};