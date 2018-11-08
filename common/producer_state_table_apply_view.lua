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
