local rets = {}
-- pop Key, Value and OP together.
local popsize = ARGV[1] * 3
local keys   = redis.call('LRANGE', KEYS[1], -popsize, -1)

redis.call('LTRIM', KEYS[1], 0, -popsize-1)

local n = table.getn(keys)
for i = n, 1, -3 do
   local op = keys[i-2]
   local value = keys[i-1]
   local key = keys[i]
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

   if ARGV[2] == "0" then
       -- do nothing, we don't want to modify redis during pop
   elseif op == 'bulkset' or op == 'bulkcreate' or op == 'bulkremove' then

-- key is "OBJECT_TYPE:num", extract object type from key
       key = key:sub(1, string.find(key, ':') - 1)

       local len = #ret
       local st = 3         -- since 1 and 2 is key/op
       while st <= len do
           local field = ret[st]
-- keyname is ASIC_STATE : OBJECT_TYPE : OBJECT_ID
           local keyname = KEYS[2] .. ':' .. key .. ':' .. field

           if op == 'bulkremove' then
               redis.call('DEL', keyname)
           else
-- value can be multiple a=v|a=v|... we need to split using gmatch
               local vars = ret[st+1]
               for value in string.gmatch(vars,'([^|]+)') do
                   local attr = value:sub(1, string.find(value, '=') - 1)
                   local val = value.sub(value, string.find(value, '=') + 1)
                   redis.call('HSET', keyname, attr, val)
               end
           end

           st = st + 2
       end

   elseif
       op == 'set' or
       op == 'SET' or
       op == 'create' or
       op == 'remove' or
       op == 'DEL' then
   -- put entries into REDIS hash only when operations are this types
   -- in case of delete command, remove entries
       local keyname = KEYS[2] .. ':' .. key
       if key == '' then
           keyname = KEYS[2]
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
   elseif
       op == 'flush' or
       op == 'flushresponse' or
       op == 'get' or
       op == 'bulkget' or
       op == 'getresponse' or
       op == 'notify' or
       op == 'get_stats' or
       op == 'clear_stats' or
       op == 'attribute_capability_query' or
       op == 'attribute_capability_response' or
       op == 'attr_enum_values_capability_query' or
       op == 'attr_enum_values_capability_response' or
       op == 'object_type_get_availability_query' or
       op == 'object_type_get_availability_response' or
       op == 'stats_capability_query' or
       op == 'stats_capability_response' then

    -- do not modify db entries when spotted those commands, they are used to
    -- trigger actions or get data synchronously from database
    -- also force to allow only those commands, so when new command will be
    -- added it will force programmer to support this command also here in
    -- right way
   else
    -- notify redis that this command is not supported and require handling
       error("unsupported operation command: " .. op .. ", FIXME")
   end
end

return rets
