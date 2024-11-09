/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_TIMED_H
#define INC_3DGS_ADVGI_TIMED_H

#include <gfx.h>

class Timed {
public:
    Timed(const std::string & name);
    ~Timed();

    inline const std::vector<std::pair<GfxTimestampQuery, std::string>> & GetTimedSections() const {
        return timed_sections_;
    }

    std::vector<std::pair<std::string, float>> CollectTimedSections () ;

    friend class TimedSection;
protected:
    std::string name_;
    int query_count_ {};
    std::vector<std::pair<GfxTimestampQuery, std::string>> timed_sections_;
};

class TimedSection {
public:
    TimedSection(Timed & timed, const char *name);
    ~TimedSection();
protected:
    Timed & timed_;
    int query_index_ {};
};


#endif //INC_3DGS_ADVGI_TIMED_H
