local tabos = require("tabos")
for key, value in pairs(assert(tabos.info())) do print(key, value) end
print(os.date("!%Y-%m-%d %H:%M:%S UTC"))
local start = tabos.monotonic_ms()
assert(tabos.sleep_ms(100))
print("Elapsed ms:", tabos.monotonic_ms() - start)
