local keys = redis.call("KEYS", "PORT|*")
table.sort(keys, function(a, b)
    local ai = tonumber(a:match("^PORT|Ethernet(%d+)$")) or math.huge
    local bi = tonumber(b:match("^PORT|Ethernet(%d+)$")) or math.huge
    return ai < bi or (ai == bi and a < b)
end)

local include_index = ARGV[1] == "-i"
local rows = {}
for _, key in ipairs(keys) do
    local name = key:sub(6)
    local values = redis.call("HMGET", key, "lanes", "alias", "speed", "index")
    local row = {name, values[1], values[2] or name, values[3]}
    if include_index then
        if not values[4] or values[4] == "" then
            return redis.error_reply("MISSING_PORT_INDEX Missing port index: " .. name)
        end
        table.insert(row, 4, values[4])
    end
    table.insert(rows, table.concat(row, "\t"))
end
return rows
