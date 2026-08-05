local mp = require("mp")

local menu_active = false
local current_items = {}
local current_selection = 1
local osd_overlay = mp.create_osd_overlay("ass-events")

mp.register_script_message("succubid_ping", function()
	mp.commandv("script-message", "succubid_pong")
end)

local function escape_ass(text)
	if not text then
		return ""
	end
	text = text:gsub("\\", "\\\239\187\191")
	text = text:gsub("{", "\\{")
	text = text:gsub("}", "\\}")
	return text
end

local function draw_menu()
	if not menu_active then
		return
	end

	local ass = "{\\an7}{\\pos(30,30)}{\\fs22}{\\bord14}{\\3c&H0A0A0A&}{\\3a&H15&}"

	ass = ass .. "{\\fs26}{\\b1}{\\1c&H00CCFF&}Multiple Funscripts Found{\\b0}{\\fs22}\\N"
	ass = ass .. "{\\1c&HB0B0B0&}Select which script to sync with The Handy:{\\r\\bord14\\3c&H0A0A0A&\\3a&H15&}\\N\\N"

	for i, item in ipairs(current_items) do
		local display_name = escape_ass(item)
		local prefix_num = (i <= 9) and ("[" .. i .. "] ") or "    "

		if i == current_selection then
			ass = ass
				.. "{\\1c&H00FFFF&}{\\b1}▶ "
				.. prefix_num
				.. display_name
				.. "{\\r\\bord14\\3c&H0A0A0A&\\3a&H15&}\\N"
		else
			ass = ass .. "  {\\1c&HEEEEEE&}" .. prefix_num .. display_name .. "\\N"
		end
	end

	ass = ass .. "\\N{\\fs16}{\\b1}{\\1c&HFFFFFF&}"
	ass = ass .. "{\\1c&H00FFFF&}[▲/▼]{\\1c&HFFFFFF&} Navigate  |  "
	ass = ass .. "{\\1c&H00FFFF&}[ENTER]{\\1c&HFFFFFF&} Select  |  "
	ass = ass .. "{\\1c&H00FFFF&}[1-9]{\\1c&HFFFFFF&} Quick Pick  |  "
	ass = ass .. "{\\1c&H00FFFF&}[ESC]{\\1c&HFFFFFF&} Cancel"

	osd_overlay.data = ass
	osd_overlay:update()
end

local function close_menu()
	menu_active = false
	osd_overlay:remove()

	mp.remove_key_binding("succubid_up")
	mp.remove_key_binding("succubid_down")
	mp.remove_key_binding("succubid_confirm")
	mp.remove_key_binding("succubid_cancel")

	for i = 1, 9 do
		mp.remove_key_binding("succubid_num_" .. i)
	end
end

local function confirm_selection(index)
	local selected_idx = index or (current_selection - 1)
	close_menu()

	mp.commandv("script-message", "selection_response", tostring(selected_idx))
end

mp.register_script_message("show_selection_menu", function(...)
	local args = { ... }
	if #args == 0 then
		return
	end

	current_items = args
	current_selection = 1
	menu_active = true

	mp.add_forced_key_binding("UP", "succubid_up", function()
		current_selection = (current_selection > 1) and (current_selection - 1) or #current_items
		draw_menu()
	end)

	mp.add_forced_key_binding("DOWN", "succubid_down", function()
		current_selection = (current_selection < #current_items) and (current_selection + 1) or 1
		draw_menu()
	end)

	mp.add_forced_key_binding("ENTER", "succubid_confirm", function()
		confirm_selection()
	end)

	mp.add_forced_key_binding("ESC", "succubid_cancel", function()
		confirm_selection(-1)
	end)

	for i = 1, math.min(#current_items, 9) do
		mp.add_forced_key_binding(tostring(i), "succubid_num_" .. i, function()
			confirm_selection(i - 1)
		end)
	end

	draw_menu()
end)
