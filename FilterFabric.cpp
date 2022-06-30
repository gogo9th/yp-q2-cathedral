
#include <array>

#include <boost/algorithm/string.hpp>

#include "utils.h"
#include "log.hpp"
#include "FilterFabric.h"
#include "DNSE_CH.h"
#include "DNSE_EQ.h"
#include "DNSE_3D.h"
#include "DNSE_BE.h"
#include "DbReduce.h"


template<size_t size, typename charType>
inline static std::array<int16_t, size> getInts(const std::vector<std::basic_string<charType>> & params)
{
    std::array<int16_t, size> values;
    int i = 0;
    for (auto & param : params)
    {
        values[i++] = std::stoi(param);
    }
    return values;
}


bool FilterFabric::addDesc(std::string desc, bool doDbReduce)
{
    try
    {
        if (boost::iequals(desc, "ballad"))
        {
            desc = "eq,12,10,16,12,14,12,10";
        }
        else if (boost::iequals(desc, "club"))
        {
            desc = "eq,19,17,9,7,15,19,18";
        }
        else if (boost::iequals(desc, "rnb"))
        {
            desc = "eq,13,19,15,13,13,15,11";
        }

        auto params = stringSplit(desc, ",");
        if (params.empty())
        {
            return false;
        }

        auto filterName = params.front();
        params.erase(params.begin());

        if (boost::iequals(filterName, "ch"))
        {
            if (doDbReduce)
            {
                filterCtors_.push_back(std::bind([] (int /*sr*/) { return std::make_unique<DbReduce>(9); }, std::placeholders::_1));
            }

            int a1 = params.size() > 0 ? std::stoi(params[0]) : 10;
            int a2 = params.size() > 1 ? std::stoi(params[1]) : 9;
            filterCtors_.push_back(std::bind([] (int a1, int a2, int sr)
                                             {
                                                 return std::make_unique<DNSE_CH>(a1, a2, sr);
                                             }, a1, a2, std::placeholders::_1));
        }
        else if (boost::iequals(filterName, "eq"))
        {
            if (params.size() < 7)
            {
                err() << "too few parameters for EQ filter";
                return false;
            }

            if (doDbReduce)
            {
                filterCtors_.push_back(std::bind([] (int /*sr*/) { return std::make_unique<DbReduce>(6); }, std::placeholders::_1));
            }

            auto gains = getInts<7>(params);
            filterCtors_.push_back(std::bind([] (auto a1, int sr)
                                             {
                                                 return std::make_unique<DNSE_EQ>(a1, sr);
                                             }, gains, std::placeholders::_1));
        }
        else if (boost::iequals(filterName, "3d"))
        {
            if (params.size() < 3)
            {
                err() << "too few parameters for 3D filter";
                return false;
            }

            //if (doDbReduce)
            //{
            //    filterCtors_.push_back(std::bind([] (int /*sr*/){ return std::make_unique<DbReduce>(6); }, std::placeholders::_1));
            //}

            auto intParams = getInts<3>(params);
            filterCtors_.push_back(std::bind([] (auto a1, auto a2, auto a3, int sr)
                                             {
                                                 return std::make_unique<DNSE_3D>(a1, a2, a3, sr);
                                             }, intParams[0], intParams[1], intParams[2], std::placeholders::_1));
        }
        else if (boost::iequals(filterName, "be"))
        {
            if (params.size() < 2)
            {
                err() << "too few parameters for 3D filter";
                return false;
            }

            auto intParams = getInts<2>(params);
            filterCtors_.push_back(std::bind([] (auto a1, auto a2, int sr)
                                             {
                                                 return std::make_unique<DNSE_BE>(a1, a2, sr);
                                             }, intParams[0], intParams[1], std::placeholders::_1));
        }
        else
        {
            err() << "unsupported filter " << filterName;
            return false;
        }

        return true;
    }
    catch (const std::exception & e)
    {
        err() << "invalid filter: " << desc << " (" << e.what() << ')';
    }
    return false;
}

std::vector<std::unique_ptr<Filter>> FilterFabric::create(int sampleRate) const
{
    std::vector<std::unique_ptr<Filter>> r;
    for (auto & ctor : filterCtors_)
    {
        r.emplace_back(std::move(ctor(sampleRate)));
    }
    return r;
}

