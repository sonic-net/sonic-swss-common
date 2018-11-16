--[[
Sample args format:
KEYS:
   SAMPLE_CHANNEL
   SAMPLE_KEY_SET
   SAMPLE_DEL_KEY_SET
   _SAMPLE:key_0
   _SAMPLE:key_1
 ARGV:
   G   (String to be published to channel)
   2   (Count of objects to set)
   key_0
   key_1
   0   (Count of objects to del)
   2   (Count of A/V pair of object 0)
   attribute_0
   value_0
   attribute_1
   value_1
   1   (Count of A/V pair of object 1)
   attribute_0
   value_0
]]
local arg_start = 2
for i = 1, ARGV[arg_start] do
    redis.call('SADD', KEYS[2], ARGV[arg_start + i])
end
arg_start = arg_start + ARGV[arg_start] + 1
for i = 1, ARGV[arg_start] do
    redis.call('SADD', KEYS[3], ARGV[arg_start + i])
end
arg_start = arg_start + ARGV[arg_start] + 1
for j = 4, #KEYS do
    for i = 1, ARGV[arg_start] do
        redis.call('HSET', KEYS[j], ARGV[arg_start + i * 2 - 1], ARGV[arg_start + i * 2])
    end
    arg_start = arg_start + 2 * ARGV[arg_start] + 1
end
redis.call('PUBLISH', KEYS[1], ARGV[1])
