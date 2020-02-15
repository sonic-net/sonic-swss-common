redis.log(redis.LOG_NOTICE, "swid", KEYS[1], "bvid", KEYS[2], "port", KEYS[3], "type", KEYS[4])

local swid = KEYS[1]
local bvid = KEYS[2]
local port = KEYS[3]
local type = KEYS[4]

if swid == "oid:0x0" then swid = "" end
if bvid == "oid:0x0" then bvid = "" end
if port == "oid:0x0" then port = "" end

redis.log(redis.LOG_NOTICE, "swid '" .. swid .. "' bvid '" .. bvid .. "' port '" .. port .. "' type '" .. type .. "'")

local pattern = "ASIC_STATE:SAI_OBJECT_TYPE_FDB_ENTRY:*bvid*" .. bvid .. "*switch_id*" .. swid .. "*";
local keys = redis.call('KEYS', pattern);
local n = table.getn(keys)

redis.log(redis.LOG_WARNING, "n = " .. n .. " pattern", pattern)

for i = 1, n do

    -- redis.log(redis.LOG_WARNING, "k = ", keys[i])

    if port == "" and type == "" then
        redis.call('DEL', keys[i])  
    elseif port == "" and type ~= "" then
        local etype = redis.call('HGET', keys[i], "SAI_FDB_ENTRY_ATTR_TYPE");
        if etype == type then
            redis.call('DEL', keys[i])  
        end
    elseif port ~= "" and type == "" then
        local eport = redis.call('HGET', keys[i], "SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID");
        if eport == port then
            redis.call('DEL', keys[i])  
        end
    else -- port =~ "" and type =~ ""
        local etype = redis.call('HGET', keys[i], "SAI_FDB_ENTRY_ATTR_TYPE");
        local eport = redis.call('HGET', keys[i], "SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID");
        if eport == port and etype == type then
            redis.call('DEL', keys[i])  
        end
    end
end
