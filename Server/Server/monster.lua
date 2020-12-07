myid = 99999;
encountered = false;

function set_uid(x)
	myid = x;
end

function event_player_move(player)
	player_x = API_get_x (player);
	player_y = API_get_y (player);
	my_x = API_get_x(myid);
	my_y = API_get_y(myid);
	if (player_x == my_x ) then
		if (player_y == my_y ) then
			API_SendMessage(myid , player, "Hello");
			encountered = true;
		end
	end
	if(encountered == true) then
		if(player_x ~= my_x or player_y ~= my_y) then
			API_SendMessage(myid , player, "Bye");
			encountered = false;
		end
	end
end