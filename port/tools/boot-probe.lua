-- Read-only CPU/display evidence and bounded original cabinet inputs.
-- No game RAM patches. All output stays in the explicitly supplied directory.
local root = assert(os.getenv('SILENT_SCOPE_PROBE_DIR'))
local log = assert(io.open(root .. '/events.txt', 'w'))
local machine = manager.machine
local fields = {}
for tag, port in pairs(machine.ioport.ports) do
    for name, field in pairs(port.fields) do
        log:write(string.format('PORT %s | %s | %x\n', tag, name, field.mask))
        fields[name] = field
    end
end
for tag, screen in pairs(machine.screens) do
    log:write(string.format('SCREEN %s %dx%d\n', tag, screen.width, screen.height))
end
local function set(name, value) if fields[name] then fields[name]:set_value(value) end end
local last = -1
silent_scope_probe_subscription = emu.add_machine_frame_notifier(function()
    local time = machine.time:as_double()
    local whole = math.floor(time)
    if whole ~= last then
        last = whole
        log:write(string.format('TIME %.4f PC %x\n', time, machine.devices[':maincpu'].state['PC'].value))
        if os.getenv('SILENT_SCOPE_CAMERA_PROBE') and (whole==35 or whole==105 or whole==110) then
            for _,tag in ipairs({':dsp1',':dsp2'}) do
                local dsp=machine.devices[tag]
                if dsp then for _,space in ipairs({'program','data'}) do
                    local output=assert(io.open(root..string.format('/%s-%s-%03d.bin',tag:sub(2),space,whole),'wb'))
                    output:write(dsp.spaces[space]:read_range(0x20000,0x2ffff,space=='program' and 64 or 32));output:close()
                end end
            end
        end
        if whole == 1 or whole == 5 or whole % 10 == 0 or ((os.getenv('SILENT_SCOPE_BOOKKEEPING') or os.getenv('SILENT_SCOPE_OPERATOR')) and whole >= 44 and whole <= 56) then
            for tag, screen in pairs(machine.screens) do
                local name = tag:gsub('[^%w_-]', '')
                screen:snapshot(root .. string.format('/%s-%03d.png', name, whole))
            end
        end
        log:flush()
    end
    if os.getenv('SILENT_SCOPE_OPERATOR') == 'scope' then
        set('Coin 1',0)
        local select_scope = time >= 34 and time < 37 and (time-34) % 1 < .3
        local select_save = time >= 44 and time < 49 and (time-44) % 1 < .3
        set('Gun Trigger', (select_scope or select_save) and 1 or 0)
        set('1 Player Start', ((time >= 39 and time < 39.3) or (time >= 52 and time < 52.3)) and 1 or 0)
    elseif os.getenv('SILENT_SCOPE_OPERATOR') == 'gun' then
        set('Coin 1', 0)
        if time >= 41 and time < 47 then set('AD Stick X',0);set('AD Stick Y',2047)
        elseif time >= 47 and time < 53 then set('AD Stick X',2047);set('AD Stick Y',0)
        else set('AD Stick X',1024);set('AD Stick Y',1024) end
        local fire = (time >= 34 and time < 34.3) or (time >= 43 and time < 43.3) or
                     (time >= 49 and time < 49.3) or (time >= 55 and time < 55.3)
        set('Gun Trigger', fire and 1 or 0)
        set('1 Player Start', time >= 37 and time < 37.3 and 1 or 0)
    elseif os.getenv('SILENT_SCOPE_BOOKKEEPING') then
        -- Service main menu: ten fresh pulls select Book Keeping; Start executes.
        set('Coin 1', 0)
        local select_book = time >= 34 and time < 44 and (time - 34) % 1 < .3
        local select_save = time >= 48 and time < 51 and (time - 48) % 1 < .3
        local select_exit = time >= 58 and time < 60 and (time - 58) % 1 < .3
        set('Gun Trigger', (select_book or select_save or select_exit) and 1 or 0)
        set('1 Player Start', ((time >= 46 and time < 46.3) or (time >= 53 and time < 53.3) or (time >= 62 and time < 62.3)) and 1 or 0)
    else
        set('Coin 1', time > 70 and time % 10 < 0.2 and 1 or 0)
        set('1 Player Start', time > 70 and time % 10 > 0.5 and time % 10 < 0.7 and 1 or 0)
        set('Gun Trigger', time > 80 and time % 1 < 0.12 and 1 or 0)
    end
    -- A short native Test pulse permits RTC initialization when needed.
    local service_time = tonumber(os.getenv('SILENT_SCOPE_TEST_TIME')) or 16
    local leave_service = os.getenv('SILENT_SCOPE_BOOKKEEPING') and time > 66 and time < 66.2
    set('Service Mode', ((time > service_time and time < service_time + .2) or
                       (time > service_time + 12 and time < service_time + 12.2) or leave_service) and 1 or 0)
end)
