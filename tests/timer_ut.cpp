#include "common/select.h"
#include "common/selectabletimer.h"
#include "gtest/gtest.h"

using namespace std;
using namespace swss;

TEST(TIMER, selectabletimer)
{
    timespec interval = { .tv_sec = 1, .tv_nsec = 0 };
    SelectableTimer timer(interval);

    Select s;
    s.addSelectable(&timer);
    Selectable *sel;
    int result;

    // Wait a non started timer
    result = s.select(&sel, 2000);
    ASSERT_EQ(result, Select::TIMEOUT);

    // Wait long enough so we got timer notification first
    timer.start();
    result = s.select(&sel, 2000);
    ASSERT_EQ(result, Select::OBJECT);
    ASSERT_EQ(sel, &timer);

    // Wait short so we got select timeout first
    result = s.select(&sel, 10);
    ASSERT_EQ(result, Select::TIMEOUT);

    // Wait long enough so we got timer notification first
    result = s.select(&sel, 10000);
    ASSERT_EQ(result, Select::OBJECT);
    ASSERT_EQ(sel, &timer);

    // Reset and wait long enough so we got timer notification first
    timer.reset();
    result = s.select(&sel, 10000);
    ASSERT_EQ(result, Select::OBJECT);
    ASSERT_EQ(sel, &timer);

    // Check if the timer gets reset by subsequent timer.start()
    for (int t = 0; t < 2000; ++t)
    {
        timer.start();
        usleep(1000);
    }
    result = s.select(&sel, 1);
    ASSERT_EQ(result, Select::OBJECT);
    ASSERT_EQ(sel, &timer);
}
