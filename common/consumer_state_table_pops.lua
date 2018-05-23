redis.replicate_commands()
local ret = {}
local keys = redis.call('SPOP', KEYS[1], ARGV[1])
local n = table.getn(keys)
for i = 1, n do
   local key = keys[i]
   local values = redis.call('HGETALL', KEYS[2] .. key)
   table.insert(ret, {key, values})
   redis.call('DEL', KEYS[2] .. key)
end
return ret
