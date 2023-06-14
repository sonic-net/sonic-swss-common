
local counters_db = ARGV[1]
local gb_counters_db = ARGV[2]
local counters_table = ARGV[3]
local separator = ARGV[4]
local operator = ARGV[5]

local gb_counter_list= {
    SAI_PORT_STAT_IF_IN_ERRORS = {'SAI_PORT_STAT_IF_OUT_ERRORS', 'SAI_PORT_STAT_IF_IN_ERRORS'},
    SAI_PORT_STAT_IF_IN_DISCARDS = {'SAI_PORT_STAT_IF_OUT_DISCARDS', 'SAI_PORT_STAT_IF_IN_DISCARDS'},
    SAI_PORT_STAT_IF_OUT_ERRORS = {'SAI_PORT_STAT_IF_IN_ERRORS', 'SAI_PORT_STAT_IF_OUT_ERRORS'},
    SAI_PORT_STAT_IF_OUT_DISCARDS = {'SAI_PORT_STAT_IF_IN_DISCARDS', 'SAI_PORT_STAT_IF_OUT_DISCARDS'},
    SAI_PORT_STAT_ETHER_RX_OVERSIZE_PKTS = {'SAI_PORT_STAT_ETHER_TX_OVERSIZE_PKTS', 'SAI_PORT_STAT_ETHER_RX_OVERSIZE_PKTS'},
    SAI_PORT_STAT_ETHER_TX_OVERSIZE_PKTS = {'SAI_PORT_STAT_ETHER_RX_OVERSIZE_PKTS', 'SAI_PORT_STAT_ETHER_TX_OVERSIZE_PKTS'},
    SAI_PORT_STAT_ETHER_STATS_UNDERSIZE_PKTS = {'', 'SAI_PORT_STAT_ETHER_STATS_UNDERSIZE_PKTS'},
    SAI_PORT_STAT_ETHER_STATS_JABBERS = {'', 'SAI_PORT_STAT_ETHER_STATS_JABBERS'},
    SAI_PORT_STAT_ETHER_STATS_FRAGMENTS = {'', 'SAI_PORT_STAT_ETHER_STATS_FRAGMENTS'},
    SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES = {'', 'SAI_PORT_STAT_IF_IN_FEC_NOT_CORRECTABLE_FRAMES'},
    SAI_PORT_STAT_IF_IN_FEC_SYMBOL_ERRORS = {'', 'SAI_PORT_STAT_IF_IN_FEC_SYMBOL_ERRORS'}
}

local function get_gbcounter(counter_id)
    local r = 0
    local counter = gb_counter_list[counter_id]
    local num
    if counter then
        for i,id in ipairs(counter) do
            if #id > 0 then
                num = redis.call('HGET', counters_table .. separator .. KEYS[i+1], id)
                if num then
                    r = r + num
                end
            end
        end
    end
    return r
end


-- KEYS:  (portID, portID_systemSide, portID_lineSide)
if #KEYS < 3 then
    return nil
end

if operator == "HGET" then
    local field = ARGV[6]
    redis.call('SELECT', counters_db)
    local counter = redis.call('HGET', counters_table .. separator .. KEYS[1], field)

    if counter then
        redis.call('SELECT', gb_counters_db)
        counter = counter + get_gbcounter(field)
        return tostring(counter)
    end
elseif operator == "HGETALL" then
    redis.call('SELECT', counters_db)
    local counter_list = redis.call('HGETALL', counters_table .. separator .. KEYS[1])

    if counter_list then
        redis.call('SELECT', gb_counters_db)
        for j = 1, #counter_list, 2 do
            counter_list[j+1] = tostring(counter_list[j+1] + get_gbcounter(counter_list[j]))
        end
        return counter_list
    end
end

return nil
