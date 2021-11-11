#include "swss/fakes/fake_sonic_db_table.h"

#include "absl/status/statusor.h"
#include "glog/logging.h"

namespace swss
{

void FakeSonicDbTable::InsertTableEntry(const std::string &key, const SonicDbEntry &values)
{
    VLOG(1) << "Insert table entry: " << key;
    entries_[key] = values;
    notifications_.push(key);
}

void FakeSonicDbTable::DeleteTableEntry(const std::string &key)
{
    VLOG(1) << "Delete table entry: " << key;
    if (auto iter = entries_.find(key); iter != entries_.end())
    {
        entries_.erase(iter);
        notifications_.push(key);
    }
}

void FakeSonicDbTable::SetResponseForKey(const std::string &key, const std::string &code, const std::string &message)
{
    VLOG(1) << "Setting response for: " << key;
    responses_[key] = ResponseInfo{.code = code, .message = message};
}

void FakeSonicDbTable::GetNextNotification(std::string &op, std::string &data, SonicDbEntry &values)
{
    if (notifications_.empty())
    {
        // TODO(rhalstea): we probably want to return a timeout error if we never
        // get a notification?
        LOG(FATAL) << "Could not find a notification.";
    }

    VLOG(1) << "Reading next notification: " << notifications_.front();
    data = notifications_.front();
    notifications_.pop();

    // If the user has overwritten the default response with custom values we will
    // use those. Otherwise, we default to success.
    if (auto response_iter = responses_.find(data); response_iter != responses_.end())
    {
        op = response_iter->second.code;
        values.push_back({"err_str", response_iter->second.message});
    }
    else
    {
        op = "SWSS_RC_SUCCESS";
        values.push_back({"err_str", "SWSS_RC_SUCCESS"});
    }
}

absl::StatusOr<SonicDbEntry> FakeSonicDbTable::ReadTableEntry(const std::string &key) const
{
    VLOG(1) << "Read table entry: " << key;
    if (auto entry = entries_.find(key); entry != entries_.end())
    {
        return entry->second;
    }
    return absl::Status(absl::StatusCode::kNotFound, absl::StrCat("AppDb missing: ", key));
}

std::vector<std::string> FakeSonicDbTable::GetAllKeys() const
{
    std::vector<std::string> result;
    VLOG(1) << "Get all table entry keys";
    for (const auto &entry : entries_)
    {
        result.push_back(entry.first);
    }
    return result;
}

void FakeSonicDbTable::DebugState() const
{
    for (const auto &[key, values] : entries_)
    {
        LOG(INFO) << "AppDb entry: " << key;
        for (const auto &[field, data] : values)
        {
            LOG(INFO) << "  " << field << " " << data;
        }
    }
}

} // namespace swss
