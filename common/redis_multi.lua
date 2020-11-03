local op = ARGV[1]
local jj = cjson.decode(KEYS[1])

if op == "mhset" then

    for keyname,o in pairs(jj) do
        for k,v in pairs(o) do
            redis.call('HSET', keyname, k, v)
        end
    end

elseif op == "mdel" then

    for idx,keyname in ipairs(jj) do
        redis.call('DEL', keyname)
    end

else
    error("unsupported operation command: " .. op .. ", FIXME")
end
