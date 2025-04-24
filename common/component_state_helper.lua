-- Copyright 2025 Google LLC
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--      http://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

-- This script updates the component state.
-- This script takes the following keys and arguments:
-- KEYS[1]: component state table + table separator. Should be 'COMPONENT_STATE_TABLE|'.
-- ARGV[1]: component ID.
-- ARGV[2]: state. Can be INITIALIZING, UP, MINOR, ERROR, or IDLE.
-- ARGV[3]: reason.
-- ARGV[4]: timestamp. A human readable timestamp in the format of "yyyy-MM-dd HH:mm:ss".
-- ARGV[5]: timestamp-seconds. An integer that represents the tv_sec field in timespec.
-- ARGV[6]: timestamp-nanoseconds. An integer that represents the tv_nsec field in timespec.
-- ARGV[7]: hardware error or not. Can be "true" or "false".
-- ARGV[8]: essential component or not. Can be "true" or "false".
-- Returns 3 entries:
-- The first entry is a boolean string.
-- "false" indicates that the state did not get updated.
-- "true" indicates that the state got updated.
-- The second entry is the previous component state. "EMPTY" if not found.
-- The third entry is the current component state.

redis.replicate_commands()
local statetablekey = KEYS[1]..ARGV[1]
local newstate = ARGV[2]
local reason = ARGV[3]
local timestamp = ARGV[4]
local timestampsecond = ARGV[5]
local timestampnanosecond = ARGV[6]
local hwerr = ARGV[7]
local essential = ARGV[8]
local update = false
local oldstate = redis.call('HGET', statetablekey, 'state')
if oldstate == false then
   oldstate = 'EMPTY'
end
if oldstate == 'INITIALIZING' then
   if newstate == 'INITIALIZING' or newstate == 'UP' or newstate == 'MINOR' or newstate == 'ERROR' or newstate == 'INACTIVE' then
      update = true
   end
elseif oldstate == 'UP' then
   if newstate == 'INITIALIZING' or newstate == 'UP' or newstate == 'MINOR' or newstate == 'ERROR' or newstate == 'INACTIVE' then
      update = true
   end
elseif oldstate == 'MINOR' then
   if newstate == 'ERROR' or newstate == 'INACTIVE' then
      update = true
   end
elseif oldstate == 'ERROR' then
   if newstate == 'INACTIVE' then
      update = true
   end
elseif oldstate == 'INACTIVE' then
   update = false
else
   if newstate == 'INITIALIZING' or newstate == 'UP' or newstate == 'MINOR' or newstate == 'ERROR' or newstate == 'INACTIVE' then
      update = true
   end
end
if update then
   local fvs = {}
   table.insert(fvs, 'state')
   table.insert(fvs, newstate)
   table.insert(fvs, 'reason')
   table.insert(fvs, reason)
   table.insert(fvs, 'timestamp')
   table.insert(fvs, timestamp)
   table.insert(fvs, 'timestamp-seconds')
   table.insert(fvs, timestampsecond)
   table.insert(fvs, 'timestamp-nanoseconds')
   table.insert(fvs, timestampnanosecond)
   table.insert(fvs, 'hw-err')
   table.insert(fvs, hwerr)
   table.insert(fvs, 'essential')
   table.insert(fvs, essential)
   redis.call('HMSET', statetablekey, unpack(fvs))
   return {'true', oldstate, newstate}
else
   return {'false', oldstate, oldstate}
end
