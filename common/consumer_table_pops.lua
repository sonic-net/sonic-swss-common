local rets = {}
local keys   = redis.call('LRANGE', KEYS[1], -ARGV[1], -1)
local ops    = redis.call('LRANGE', KEYS[2], -ARGV[1], -1)
local values = redis.call('LRANGE', KEYS[3], -ARGV[1], -1)

redis.call('LTRIM', KEYS[1], 0, -ARGV[1]-1)
redis.call('LTRIM', KEYS[2], 0, -ARGV[1]-1)
redis.call('LTRIM', KEYS[3], 0, -ARGV[1]-1)

local n = table.getn(keys)
for i = n, 1, -1 do
   local key = keys[i]
   local op = ops[i]
   local value = values[i]
   local dbop = op:sub(1,1)
   op = op:sub(2)
   local ret = {key, op}

   local jj = cjson.decode(value)
   local size = #jj

   for idx=1,size,2 do
       table.insert(ret, jj[idx])
       table.insert(ret, jj[idx+1])
   end
   table.insert(rets, ret)

   if op == 'bulkset' or op == 'bulkcreate' then

-- key is "OBJECT_TYPE:num", extract object type from key
       key = key:sub(1, string.find(key, ':') - 1)

       local len = #ret
       local st = 3         -- since 1 and 2 is key/op
       while st <= len do
           local field = ret[st]
-- keyname is ASIC_STATE : OBJECT_TYPE : OBJECT_ID
           local keyname = KEYS[4] .. ':' .. key .. ':' .. field

-- value can be multiple a=v|a=v|... we need to split using gmatch
           local vars = ret[st+1]
           for value in string.gmatch(vars,'([^|]+)') do
               local attr = value:sub(1, string.find(value, '=') - 1)
               local val = value.sub(value, string.find(value, '=') + 1)
               redis.call('HSET', keyname, attr, val)
           end

           st = st + 2
       end

   elseif op ~= 'get' and op ~= 'getresponse' and op ~= 'notify' then
       local keyname = KEYS[4] .. ':' .. key
       if key == '' then
           keyname = KEYS[4]
       end

       if dbop == 'D' then
           redis.call('DEL', keyname)
       else
           local st = 3
           local len = #ret
           while st <= len do
               redis.call('HSET', keyname, ret[st], ret[st+1])
               st = st + 2
           end
       end
   end
end

return rets
