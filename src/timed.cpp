/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */

#include "timed.h"
#include "app_internal.h"

Timed::Timed (const std::string & name): name_(name) {}

Timed::~Timed() {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    for(auto & pair : timed_sections_) {
        gfxDestroyTimestampQuery(gfx, pair.first);
    }
}

TimedSection::TimedSection (Timed & timed, const char *name): timed_(timed) {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    int query_index = timed.query_count_ ++;
    if(query_index >= timed.timed_sections_.size()) {
        timed.timed_sections_.resize(query_index + 1);
        timed.timed_sections_[query_index].first = gfxCreateTimestampQuery(gfx);
    }
    query_index_ = query_index;
    GfxTimestampQuery & query = timed.timed_sections_[query_index].first;
    timed.timed_sections_[query_index].second = name;
    gfxCommandBeginTimestampQuery(gfx, query);
    gfxCommandBeginEvent(gfx, name);
}

TimedSection::~TimedSection () {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxCommandEndTimestampQuery(gfx, timed_.timed_sections_[query_index_].first);
    gfxCommandEndEvent(gfx);
}

std::vector<std::pair<std::string, float>> Timed::CollectTimedSections () {
    std::vector<std::pair<std::string, float>> result;
    auto & gfx = AppInternal::GetInstance().GetGfx();
    for(int i = 0; i < query_count_; i++) {
        float duration = gfxTimestampQueryGetDuration(gfx, timed_sections_[i].first);
        result.emplace_back(timed_sections_[i].second, duration);
    }
    query_count_ = 0;
    return result;
}