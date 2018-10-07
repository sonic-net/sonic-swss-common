redis.replicate_commands()
local ret = {}
local tablename = KEYS[2]
local stateprefix = ARGV[2]
local keys = redis.call('SPOP', KEYS[1], ARGV[1])
local n = table.getn(keys)
for i = 1, n do
   local key = keys[i]
-- Check if there was request to delte the key, clear the key in table first
   local num = redis.call('SREM', KEYS[1]..'_DEL', key)
   if num == 1 then
      redis.call('DEL', tablename..key)
   end
-- Push the new set of field/value with the key in table
   local fieldvalues = redis.call('HGETALL', stateprefix..tablename..key)
   table.insert(ret, {key, fieldvalues})
   for i = 1, #fieldvalues, 2 do
      redis.call('HSET', tablename..key, fieldvalues[i], fieldvalues[i + 1])
   end
-- Clean up the key in temporary state table
   redis.call('DEL', stateprefix..tablename..key)
end
return ret
