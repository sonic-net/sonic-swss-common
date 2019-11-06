local keys = redis.call('KEYS', KEYS[1])
local n = table.getn(keys)

for i = 1, n do
   if KEYS[2] == "" and KEYS[3] == "1" then
      redis.call('DEL', keys[i])  
   elseif KEYS[2] == "" and KEYS[3] == "0" then
      local type = redis.call('HGET', keys[i], "SAI_FDB_ENTRY_ATTR_TYPE");
      if type == "SAI_FDB_ENTRY_TYPE_DYNAMIC" then
         redis.call('DEL', keys[i])  
      end
   elseif KEYS[2] ~= "" then
      local bridge_port = redis.call('HGET', keys[i], "SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID");
      if bridge_port == KEYS[2] then
         if KEYS[3] == "1" then
            redis.call('DEL', keys[i])  
         else
            local type = redis.call('HGET', keys[i], "SAI_FDB_ENTRY_ATTR_TYPE");
            if type == "SAI_FDB_ENTRY_TYPE_DYNAMIC" then
               redis.call('DEL', keys[i])  
            end
         end
      end
   end
end
