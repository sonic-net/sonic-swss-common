redis.replicate_commands()
local ret = {}
local tablename = KEYS[2]
local stateprefix = ARGV[2]
local keys = redis.call('SPOP', KEYS[1], ARGV[1])
local n = table.getn(keys)
for i = 1, n do
   local key = keys[i]
   local fieldvalues = redis.call('HGETALL', stateprefix..tablename..key)
   table.insert(ret, {key, fieldvalues})
   for f, v in pairs(fieldvalues) do
      redis.call('HSET', tablename..key, f, v)
   end
   redis.call('DEL', stateprefix..tablename..key)
end
return ret
