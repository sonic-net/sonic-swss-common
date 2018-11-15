local keys = redis.call("KEYS", KEYS[1] .. ":*")
local res = {}

for i,k in pairs(keys) do
   local sres={}

   local flat_map = redis.call('HGETALL', k)
   for j = 1, #flat_map, 2 do
       sres[flat_map[j]] = flat_map[j + 1]
   end

   res[k] = sres
end

return cjson.encode(res)
