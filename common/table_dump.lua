local keys = redis.call("keys", KEYS[1] .. ":*")
local res = {}

for i,k in pairs(keys) do

   local skeys = redis.call("HKEYS", k)
   local sres={}

   for j,sk in pairs(skeys) do
       sres[sk] = redis.call("HGET", k, sk)
   end

   res[k] = sres

end

return cjson.encode(res)
