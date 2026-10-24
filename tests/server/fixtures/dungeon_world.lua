-- Isolated engine boundary for production quest handlers. No SQL, network,
-- server processes or object/QC files. Count filtering matches the C++ API.
function reset(base)
    map=base*10000+1; current_map=map; current_pid=1; member=true; is_party=true
    flags={}; qflags={}; monsters={}; timers={}; notices={}; drops={}; warps={}
    completions={}; rewards={}; answers={}; removed=0; nextvid=100; selected=0; item_vnum=0
    spawn_failure=0; exits=0; joins=0; hp_changes=0; event_arg=map
end
local function valid(idx) assert(idx==map, 'wrong dungeon map argument: '..tostring(idx)) end
local function noop() end
function get_server_timer_arg() return event_arg end
function get_global_time() return 1000 end
function number(a,b) return b end
function server_timer(name,seconds,idx) valid(idx); assert(seconds>0); timers[name]={idx=idx,seconds=seconds} end
server_loop_timer=server_timer
function clear_server_timer(name,idx) valid(idx); timers[name]=nil end
function notice_all(...) end
function syschat(...) end
function mob_name(vnum) return tostring(vnum) end
function item_name(vnum) return tostring(vnum) end
for _,name in ipairs({'say','say_title','say_reward','addimage','setskin','wait'}) do _G[name]=noop end
NOWINDOW=0
function select(...) return table.remove(answers,1) or 1 end
gameforge=setmetatable({}, {__index=function() return setmetatable({}, {__index=function() return setmetatable({}, {__index=function() return 'text' end}) end}) end})
pc={
    in_dungeon=function() return member end, get_map_index=function() return current_map end,
    get_channel_id=function() return 1 end, set_warp_location=noop,
    setf=function(quest,key,value) qflags[current_pid..':'..quest..':'..key]=value end,
    getf=function(quest,key) return qflags[current_pid..':'..quest..':'..key] or 0 end,
    get_name=function() return 'test-player' end, get_language=function() return 0 end,
    get_player_id=function() return current_pid end, getqf=function(key) return qflags[current_pid..':'..key] or 0 end,
    setqf=function(key,value) qflags[current_pid..':'..key]=value end,
    count_item=function() return 0 end, remove_item=noop,
    give_item2=function(vnum,count) rewards[vnum]=(rewards[vnum] or 0)+count end,
    warp=function(x,y,idx) if idx then valid(idx) end; warps[#warps+1]={x=x,y=y,idx=idx} end,
    can_drop=function() return true end
}
party={is_party=function() return is_party end,is_leader=function() return false end,
    get_leader_name=function() return 'test-leader' end, give_blacksmith=noop}
guild={get_id=function() return 7 end}
game={get_event_flag=function() return -100 end,set_event_flag=noop,
    drop_item=function(vnum,count) drops[vnum]=(drops[vnum] or 0)+count end}
item={get_vnum=function() return item_vnum end,remove=function() removed=removed+1 end}
function spawn(idx,vnum,x,y)
    valid(idx)
    if spawn_failure==vnum then return 0 end
    nextvid=nextvid+1
    monsters[nextvid]={vnum=vnum,invincible=false,x=x,y=y}
    return nextvid
end
function regen(idx,path)
    valid(idx)
    if path:find('monkey_razor/floor_1.json',1,true) then
        for i=1,48 do spawn(idx,5121,150,150) end
        for i=1,25 do spawn(idx,5122,150,150) end
    elseif path:find('monkey_razor/boss.json',1,true) then
        for i=1,31 do spawn(idx,5125,625,457) end
        for i=1,16 do spawn(idx,5126,625,457) end
    end
end
d={
    find=function(idx) return idx==map end,
    getf=function(idx,key) valid(idx); return flags[key] or 0 end,
    setf=function(idx,key,value) valid(idx); flags[key]=value end,
    count_monster=function(idx,vnum)
        valid(idx);local n=0
        for _,mob in pairs(monsters) do
            if (not vnum or mob.vnum==vnum) and mob.vnum~=30111 then n=n+1 end
        end
        return n
    end,
    spawn_mob=spawn, regen_file=regen, set_regen_file=regen,clear_regen=function(idx) valid(idx) end,
    kill_all=function(idx) valid(idx);monsters={} end,
    kill_all_monsters=function(idx) valid(idx);monsters={} end,
    set_vid_invincible=function(vid,value)
        if not monsters[vid] then return false end
        monsters[vid].invincible=value; return true
    end,
    notice=function(idx,n,text) valid(idx);notices[n]=(notices[n] or 0)+1 end,
    exit_all_lobby=function(idx) valid(idx);exits=exits+1 end,
    jump_all=function(idx,x,y) valid(idx);warps[#warps+1]={x=x*100,y=y*100,idx=idx} end,
    complete=function(vnum,cooldown,base,name) completions[#completions+1]={vnum=vnum,base=base,name=name} end,
    check_entrance=function() return 1,0,'test-player' end,
    join_cords=function() joins=joins+1 end,remove_item=noop
}
npc={get_vid=function() return selected end,get_race=function() return monsters[selected] and monsters[selected].vnum or 0 end,
    purge=function() monsters[selected]=nil end, kill=function() monsters[selected]=nil end,
    npc_decrease_hp_by_percent_razor93=function(vid,percent)
        if not monsters[vid] then return false end
        hp_changes=hp_changes+1;return true
    end}
function fire(event,target)
    local handler=handlers[event..':'..(target or 'notarget')]
    assert(handler, 'missing handler '..event..':'..(target or 'notarget'))
    handler()
end
function kill(vnum)
    for vid,mob in pairs(monsters) do
        if mob.vnum==vnum then
            assert(not mob.invincible,'attempted protected objective')
            selected=vid; monsters[vid]=nil; fire('kill',tostring(vnum)); return
        end
    end
    error('missing mob '..vnum)
end
