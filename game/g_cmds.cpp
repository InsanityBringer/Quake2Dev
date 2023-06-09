/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "g_local.h"
#include "m_player.h"


char *ClientTeam (edict_t *ent)
{
	char		*p;
	static char	value[512];

	value[0] = 0;

	if (!ent->client)
		return value;

	strcpy(value, Info_ValueForKey (ent->client->pers.userinfo, "skin"));
	p = strchr(value, '/');
	if (!p)
		return value;

	if ((int)(dmflags->value) & DF_MODELTEAMS)
	{
		*p = 0;
		return value;
	}

	// if ((int)(dmflags->value) & DF_SKINTEAMS)
	return ++p;
}

qboolean OnSameTeam (edict_t *ent1, edict_t *ent2)
{
	char	ent1Team [512];
	char	ent2Team [512];

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		return qfalse;

	strcpy (ent1Team, ClientTeam (ent1));
	strcpy (ent2Team, ClientTeam (ent2));

	if (strcmp(ent1Team, ent2Team) == 0)
		return qtrue;
	return qfalse;
}


void SelectNextItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->client;

	if (cl->chase_target) {
		ChaseNext(ent);
		return;
	}

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void SelectPrevItem (edict_t *ent, int itflags)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;

	cl = ent->client;

	if (cl->chase_target) {
		ChasePrev(ent);
		return;
	}

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (cl->pers.selected_item + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (!(it->flags & itflags))
			continue;

		cl->pers.selected_item = index;
		return;
	}

	cl->pers.selected_item = -1;
}

void ValidateSelectedItem (edict_t *ent)
{
	gclient_t	*cl;

	cl = ent->client;

	if (cl->pers.inventory[cl->pers.selected_item])
		return;		// valid

	SelectNextItem (ent, -1);
}


//=================================================================================

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void Cmd_Give_f (edict_t *ent)
{
	char		*name;
	gitem_t		*it;
	int			index;
	int			i;
	qboolean	give_all;
	edict_t		*it_ent;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	name = gi.args();

	if (Q_stricmp(name, "all") == 0)
		give_all = qtrue;
	else
		give_all = qfalse;

	if (give_all || Q_stricmp(gi.argv(1), "health") == 0)
	{
		if (gi.argc() == 3)
			ent->health = atoi(gi.argv(2));
		else
			ent->health = ent->max_health;
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "weapons") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_WEAPON))
				continue;
			ent->client->pers.inventory[i] += 1;
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "ammo") == 0)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (!(it->flags & IT_AMMO))
				continue;
			Add_Ammo (ent, it, 1000);
		}
		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "armor") == 0)
	{
		gitem_armor_t	*info;

		it = FindItem("Jacket Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Combat Armor");
		ent->client->pers.inventory[ITEM_INDEX(it)] = 0;

		it = FindItem("Body Armor");
		info = (gitem_armor_t *)it->info;
		ent->client->pers.inventory[ITEM_INDEX(it)] = info->max_count;

		if (!give_all)
			return;
	}

	if (give_all || Q_stricmp(name, "Power Shield") == 0)
	{
		it = FindItem("Power Shield");
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);

		if (!give_all)
			return;
	}

	if (give_all)
	{
		for (i=0 ; i<game.num_items ; i++)
		{
			it = itemlist + i;
			if (!it->pickup)
				continue;
			if (it->flags & (IT_ARMOR|IT_WEAPON|IT_AMMO))
				continue;
			ent->client->pers.inventory[i] = 1;
		}
		return;
	}

	it = FindItem (name);
	if (!it)
	{
		name = gi.argv(1);
		it = FindItem (name);
		if (!it)
		{
			gi.cprintf (ent, PRINT_HIGH, "unknown item\n");
			return;
		}
	}

	if (!it->pickup)
	{
		gi.cprintf (ent, PRINT_HIGH, "non-pickup item\n");
		return;
	}

	index = ITEM_INDEX(it);

	if (it->flags & IT_AMMO)
	{
		if (gi.argc() == 3)
			ent->client->pers.inventory[index] = atoi(gi.argv(2));
		else
			ent->client->pers.inventory[index] += it->quantity;
	}
	else
	{
		it_ent = G_Spawn();
		it_ent->classname = it->classname;
		SpawnItem (it_ent, it);
		Touch_Item (it_ent, ent, NULL, NULL);
		if (it_ent->inuse)
			G_FreeEdict(it_ent);
	}
}

//this should probably get put in a header somewhere
void ED_CallSpawn (edict_t *ent);
void ED_ParseField(char *key, char *value, edict_t *ent);
char* ED_NewString(char* string);
void droptofloor(edict_t* ent);

/*
==================
Cmd_Shoot_f

Will I ever add a useful command?
==================
*/
void Cmd_Shoot_f(edict_t* ent)
{
	int		i;
	vec3_t	forward, src;

	int damage = -1;
	int speed = -1;
	int radiusdmg = -1;
	float timer = -1;

	char* name;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	if (gi.argc() > 1)
	{
		if (gi.argc() > 2)
		{
			damage = atoi(gi.argv(2));
		}
		if (gi.argc() > 3)
		{
			speed = atoi(gi.argv(3));
		}
		if (gi.argc() > 4)
		{
			radiusdmg = atoi(gi.argv(4));
		}
		if (gi.argc() > 5)
		{
			timer = atof(gi.argv(5));
		}
		AngleVectors(ent->client->v_angle, forward, NULL, NULL);

		name = gi.argv(1);
		if (!Q_stricmp(name, "blaster"))
		{
			if (damage == -1) damage = 10;
			if (speed == -1) speed = 1000;
			fire_blaster(ent, ent->s.origin, forward, damage, speed, EF_BLASTER, qfalse);
		}
		else if (!Q_stricmp(name, "hyperblaster"))
		{
			if (damage == -1) damage = 20;
			if (speed == -1) speed = 1000;
			fire_blaster(ent, ent->s.origin, forward, damage, speed, EF_HYPERBLASTER, qtrue);
		}
		else if (!Q_stricmp(name, "rocket"))
		{
			if (damage == -1) damage = 100 + (int)(random() * 20.0);
			if (radiusdmg == -1) radiusdmg = 120;
			if (speed == -1) speed = 650;
			fire_rocket(ent, ent->s.origin, forward, damage, speed, radiusdmg, radiusdmg);
		}
		else if (!Q_stricmp(name, "grenade"))
		{
			if (damage == -1) damage = 100 + (int)(random() * 20.0);
			if (radiusdmg == -1) radiusdmg = 120;
			if (speed == -1) speed = 600;
			if (timer < 0) timer = 2.5;
			fire_grenade(ent, ent->s.origin, forward, damage, speed, timer, radiusdmg);
		}
		else if (!Q_stricmp(name, "handgrenade"))
		{
			if (damage == -1) damage = 100 + (int)(random() * 20.0);
			if (radiusdmg == -1) radiusdmg = 120;
			if (speed == -1) speed = 600;
			if (timer < 0) timer = 2.5;
			fire_grenade2(ent, ent->s.origin, forward, damage, speed, timer, radiusdmg, qfalse);
		}
		else if (!Q_stricmp(name, "bfg"))
		{
			if (damage == -1) damage = 500;
			if (radiusdmg == -1) radiusdmg = 1000;
			if (speed == -1) speed = 400;
			fire_bfg(ent, ent->s.origin, forward, damage, speed, radiusdmg);
		}
		else
		{
			gi.cprintf(ent, PRINT_HIGH, "unknown type %s\n", gi.argv(1));
		}
	}
	else
	{
		gi.cprintf(ent, PRINT_HIGH, "usage: shoot type [damage] [speed] [radius damage] [timer].\n");
		return;
	}
}

//we need radians
#define DEG2RAD( a ) ( a * M_PI ) / 180.0F
/*
==================
Cmd_Ent_Create_f

Spawns an entity at a given spot
==================
*/
void Cmd_Ent_Create_f (edict_t *ent)
{
	char	*name;
	vec3_t	spawnorig, spawnoffs, spawnfinal;
	edict_t	*newent;
	int		numkvs;
	int		i;
	vec3_t	forward, end, src;
	vec3_t	mins, maxs;
	trace_t trace;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	if (gi.argc() > 1)
	{
		numkvs = (gi.argc() / 2) - 1;
		//gi.cprintf(ent, PRINT_HIGH, "%d keyvalue pairs\n", numkvs);
		name = gi.argv(1);

		AngleVectors(ent->client->v_angle, forward, NULL, NULL);

		//get a new entity
		newent = G_Spawn();
		newent->classname = ED_NewString(name);

		//parse any keyvalues this might have gotten
		for (i = 0; i < numkvs; i++)
		{
			ED_ParseField(gi.argv((i * 2) + 2), gi.argv((i * 2) + 3), newent);
		}

		//set up the new entity
		ED_CallSpawn(newent);

		//Check if spawning the entity failed
		if (!Q_strcasecmp("freed", newent->classname))
		{
			gi.cprintf(ent, PRINT_HIGH, "Error spawning object\n");
			return;
		}

		//hack: items don't have their bounding boxes set until later. Try to detect this
		if (newent->think == droptofloor)
		{
			VectorSet(mins, -16, -16, -16);
			VectorSet(maxs, 16, 16, 16);
		}
		else
		{
			VectorCopy(ent->mins, mins);
			VectorCopy(ent->maxs, maxs);
		}

		VectorCopy(ent->s.origin, src);
		src[2] += ent->viewheight;
		VectorMA(src, 8192, forward, end);
		trace = gi.trace(src, mins, maxs, end, ent, MASK_SHOT);

		if (trace.startsolid)
		{
			gi.cprintf(ent, PRINT_HIGH, "Can't fit new object\n");
			G_FreeEdict(newent);
			return;
		}
		gi.unlinkentity(newent);
		newent->s.origin[0] = trace.endpos[0];
		newent->s.origin[1] = trace.endpos[1];
		newent->s.origin[2] = trace.endpos[2];
		if (trace.fraction < 1.0)
			newent->s.origin[2] += 12;
		gi.linkentity(newent);

		VectorCopy(ent->s.angles, newent->s.angles);
		//don't pitch
		newent->s.angles[0] = 0;
		//don't roll either tbh
		newent->s.angles[2] = 0;

		if (!strcmp("noclass", newent->classname))
		{
			gi.cprintf(ent, PRINT_HIGH, "Unknown type %s\n", gi.argv(1));
			G_FreeEdict(newent);
		}

		//I need to double check this
		/*//if they don't fit, remove them.
		gi.unlinkentity (ent);
		KillBox (ent);
		gi.linkentity (ent);*/
	}
	else
	{
		gi.cprintf(ent, PRINT_HIGH, "usage: ent_create entityname <key> <value>...\n");
		return;
	}
}

/*
==================
Cmd_Ent_Fire_f

Triggers all entities with the specified name

argv(0) ent_fire
argv(1) targetname
==================
*/
void Cmd_Ent_Fire_f(edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}
	if (gi.argc() > 1)
	{
		char* oldtarget = ent->target;
		ent->target = gi.argv(1);
		G_UseTargets(ent, ent);
		ent->target = oldtarget;
	}
	else
	{
		gi.cprintf(ent, PRINT_HIGH, "hey actually tell me what you want targeted maybe\n");
		return;
	}
}


/*
==================
Cmd_Ent_Remove_f

Sends all entities with the given targetname to the shadow realm

argv(0) ent_fire
argv(1) targetname
==================
*/
void Cmd_Ent_Remove_f(edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}
	if (gi.argc() > 1)
	{
		char* oldtarget = ent->killtarget;
		ent->killtarget = gi.argv(1);
		G_UseTargets(ent, ent);
		ent->killtarget = oldtarget;
	}
	else
	{
		gi.cprintf(ent, PRINT_HIGH, "hey actually tell me what you want killed maybe\n");
		return;
	}
}

/*
==================
Cmd_Ent_Name_f

Spawns an entity at a given spot
==================
*/
void Cmd_Ent_Name_f(edict_t* ent)
{
	char* name;
	int		i;
	vec3_t	forward, end, src;
	trace_t trace;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf(ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}
	if (gi.argc() > 1)
	{
		name = gi.argv(1);

		AngleVectors(ent->client->v_angle, forward, NULL, NULL);

		VectorCopy(ent->s.origin, src);
		src[2] += ent->viewheight;
		VectorMA(src, 8192, forward, end);
		trace = gi.trace(src, NULL, NULL, end, ent, MASK_SHOT);

		if (trace.ent == NULL || !strcmp(trace.ent->classname, "worldspawn"))
		{
			gi.cprintf(ent, PRINT_HIGH, "Can't find entity to name.\n");
		}
		else
		{
			trace.ent->targetname = ED_NewString(name);
			gi.cprintf(ent, PRINT_HIGH, "Set targetname of %s to %s.\n", trace.ent->classname, name);
		}
	}
	else
	{
		gi.cprintf(ent, PRINT_HIGH, "No targetname specified.\n");
		return;
	}
}

/*
==================
Cmd_Mdk_f

Makes the entity you're staring at just fuckin die
==================
*/
void Cmd_Mdk_f(edict_t* ent)
{
	int		i;
	vec3_t	forward, end, src;
	trace_t trace;

	if (deathmatch->value)
	{
		gi.cprintf(ent, PRINT_HIGH, "Cannot use MDK in deathmatch.\n");
		return;
	}
	
	AngleVectors(ent->client->v_angle, forward, NULL, NULL);

	VectorCopy(ent->s.origin, src);
	src[2] += ent->viewheight;
	VectorMA(src, 8192, forward, end);
	trace = gi.trace(src, NULL, NULL, end, ent, MASK_SHOT);

	if (trace.fraction < 1.0)
	{
		if (trace.ent != NULL && strcmp(trace.ent->classname, "worldspawn") && trace.ent->takedamage)
		{
			T_Damage(trace.ent, ent, ent, forward, ent->s.origin, trace.plane.normal, 1000000, 100, 0, MOD_UNKNOWN);
		}
		else
		{
			if (strncmp(trace.surface->name, "sky", 3) != 0)
			{
				gi.WriteByte(svc_temp_entity);
				gi.WriteByte(TE_GUNSHOT);
				gi.WritePosition(trace.endpos);
				gi.WriteDir(trace.plane.normal);
				gi.multicast(trace.endpos, MULTICAST_PVS);

				if (ent->client)
					PlayerNoise(ent, trace.endpos, PNOISE_IMPACT);
			}
		}
	}
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_GODMODE;
	if (!(ent->flags & FL_GODMODE) )
		msg = "godmode OFF\n";
	else
		msg = "godmode ON\n";

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	ent->flags ^= FL_NOTARGET;
	if (!(ent->flags & FL_NOTARGET) )
		msg = "notarget OFF\n";
	else
		msg = "notarget ON\n";

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f (edict_t *ent)
{
	char	*msg;

	if (deathmatch->value && !sv_cheats->value)
	{
		gi.cprintf (ent, PRINT_HIGH, "You must run the server with '+set cheats 1' to enable this command.\n");
		return;
	}

	if (ent->movetype == MOVETYPE_NOCLIP)
	{
		ent->movetype = MOVETYPE_WALK;
		msg = "noclip OFF\n";
	}
	else
	{
		ent->movetype = MOVETYPE_NOCLIP;
		msg = "noclip ON\n";
	}

	gi.cprintf (ent, PRINT_HIGH, msg);
}


/*
==================
Cmd_Use_f

Use an inventory item
==================
*/
void Cmd_Use_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	s = gi.args();
	it = FindItem (s);
	if (!it)
	{
		gi.cprintf (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->use)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->use (ent, it);
}


/*
==================
Cmd_Drop_f

Drop an inventory item
==================
*/
void Cmd_Drop_f (edict_t *ent)
{
	int			index;
	gitem_t		*it;
	char		*s;

	s = gi.args();
	it = FindItem (s);
	if (!it)
	{
		gi.cprintf (ent, PRINT_HIGH, "unknown item: %s\n", s);
		return;
	}
	if (!it->drop)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	index = ITEM_INDEX(it);
	if (!ent->client->pers.inventory[index])
	{
		gi.cprintf (ent, PRINT_HIGH, "Out of item: %s\n", s);
		return;
	}

	it->drop (ent, it);
}


/*
=================
Cmd_Inven_f
=================
*/
void Cmd_Inven_f (edict_t *ent)
{
	int			i;
	gclient_t	*cl;

	cl = ent->client;

	cl->showscores = qfalse;
	cl->showhelp = qfalse;

	if (cl->showinventory)
	{
		cl->showinventory = qfalse;
		return;
	}

	cl->showinventory = qtrue;

	gi.WriteByte (svc_inventory);
	for (i=0 ; i<MAX_ITEMS ; i++)
	{
		gi.WriteShort (cl->pers.inventory[i]);
	}
	gi.unicast (ent, qtrue);
}

/*
=================
Cmd_InvUse_f
=================
*/
void Cmd_InvUse_f (edict_t *ent)
{
	gitem_t		*it;

	ValidateSelectedItem (ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "No item to use.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->use)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not usable.\n");
		return;
	}
	it->use (ent, it);
}

/*
=================
Cmd_WeapPrev_f
=================
*/
void Cmd_WeapPrev_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapNext_f
=================
*/
void Cmd_WeapNext_f (edict_t *ent)
{
	gclient_t	*cl;
	int			i, index;
	gitem_t		*it;
	int			selected_weapon;

	cl = ent->client;

	if (!cl->pers.weapon)
		return;

	selected_weapon = ITEM_INDEX(cl->pers.weapon);

	// scan  for the next valid one
	for (i=1 ; i<=MAX_ITEMS ; i++)
	{
		index = (selected_weapon + MAX_ITEMS - i)%MAX_ITEMS;
		if (!cl->pers.inventory[index])
			continue;
		it = &itemlist[index];
		if (!it->use)
			continue;
		if (! (it->flags & IT_WEAPON) )
			continue;
		it->use (ent, it);
		if (cl->pers.weapon == it)
			return;	// successful
	}
}

/*
=================
Cmd_WeapLast_f
=================
*/
void Cmd_WeapLast_f (edict_t *ent)
{
	gclient_t	*cl;
	int			index;
	gitem_t		*it;

	cl = ent->client;

	if (!cl->pers.weapon || !cl->pers.lastweapon)
		return;

	index = ITEM_INDEX(cl->pers.lastweapon);
	if (!cl->pers.inventory[index])
		return;
	it = &itemlist[index];
	if (!it->use)
		return;
	if (! (it->flags & IT_WEAPON) )
		return;
	it->use (ent, it);
}

/*
=================
Cmd_InvDrop_f
=================
*/
void Cmd_InvDrop_f (edict_t *ent)
{
	gitem_t		*it;

	ValidateSelectedItem (ent);

	if (ent->client->pers.selected_item == -1)
	{
		gi.cprintf (ent, PRINT_HIGH, "No item to drop.\n");
		return;
	}

	it = &itemlist[ent->client->pers.selected_item];
	if (!it->drop)
	{
		gi.cprintf (ent, PRINT_HIGH, "Item is not dropable.\n");
		return;
	}
	it->drop (ent, it);
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f (edict_t *ent)
{
	if((level.time - ent->client->respawn_time) < 5)
		return;
	ent->flags &= ~FL_GODMODE;
	ent->health = 0;
	meansOfDeath = MOD_SUICIDE;
	player_die (ent, ent, ent, 100000, vec3_origin);
}

/*
=================
Cmd_PutAway_f
=================
*/
void Cmd_PutAway_f (edict_t *ent)
{
	ent->client->showscores = qfalse;
	ent->client->showhelp = qfalse;
	ent->client->showinventory = qfalse;
}


int PlayerSort (void const *a, void const *b)
{
	int		anum, bnum;

	anum = *(int *)a;
	bnum = *(int *)b;

	anum = game.clients[anum].ps.stats[STAT_FRAGS];
	bnum = game.clients[bnum].ps.stats[STAT_FRAGS];

	if (anum < bnum)
		return -1;
	if (anum > bnum)
		return 1;
	return 0;
}

/*
=================
Cmd_Players_f
=================
*/
void Cmd_Players_f (edict_t *ent)
{
	int		i;
	int		count;
	char	small[64];
	char	large[1280];
	int		index[256];

	count = 0;
	for (i = 0 ; i < maxclients->value ; i++)
		if (game.clients[i].pers.connected)
		{
			index[count] = i;
			count++;
		}

	// sort by frags
	qsort (index, count, sizeof(index[0]), PlayerSort);

	// print information
	large[0] = 0;

	for (i = 0 ; i < count ; i++)
	{
		Com_sprintf (small, sizeof(small), "%3i %s\n",
			game.clients[index[i]].ps.stats[STAT_FRAGS],
			game.clients[index[i]].pers.netname);
		if (strlen (small) + strlen(large) > sizeof(large) - 100 )
		{	// can't print all of them in one packet
			strcat (large, "...\n");
			break;
		}
		strcat (large, small);
	}

	gi.cprintf (ent, PRINT_HIGH, "%s\n%i players\n", large, count);
}

/*
=================
Cmd_Wave_f
=================
*/
void Cmd_Wave_f (edict_t *ent)
{
	int		i;

	i = atoi (gi.argv(1));

	// can't wave when ducked
	if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
		return;

	if (ent->client->anim_priority > ANIM_WAVE)
		return;

	ent->client->anim_priority = ANIM_WAVE;

	switch (i)
	{
	case 0:
		gi.cprintf (ent, PRINT_HIGH, "flipoff\n");
		ent->s.frame = FRAME_flip01-1;
		ent->client->anim_end = FRAME_flip12;
		break;
	case 1:
		gi.cprintf (ent, PRINT_HIGH, "salute\n");
		ent->s.frame = FRAME_salute01-1;
		ent->client->anim_end = FRAME_salute11;
		break;
	case 2:
		gi.cprintf (ent, PRINT_HIGH, "taunt\n");
		ent->s.frame = FRAME_taunt01-1;
		ent->client->anim_end = FRAME_taunt17;
		break;
	case 3:
		gi.cprintf (ent, PRINT_HIGH, "wave\n");
		ent->s.frame = FRAME_wave01-1;
		ent->client->anim_end = FRAME_wave11;
		break;
	case 4:
	default:
		gi.cprintf (ent, PRINT_HIGH, "point\n");
		ent->s.frame = FRAME_point01-1;
		ent->client->anim_end = FRAME_point12;
		break;
	}
}

/*
==================
Cmd_Say_f
==================
*/
void Cmd_Say_f (edict_t *ent, qboolean team, qboolean arg0)
{
	int		i, j;
	edict_t	*other;
	char	*p;
	char	text[2048];
	gclient_t *cl;

	if (gi.argc () < 2 && !arg0)
		return;

	if (!((int)(dmflags->value) & (DF_MODELTEAMS | DF_SKINTEAMS)))
		team = qfalse;

	if (team)
		Com_sprintf (text, sizeof(text), "(%s): ", ent->client->pers.netname);
	else
		Com_sprintf (text, sizeof(text), "%s: ", ent->client->pers.netname);

	if (arg0)
	{
		strcat (text, gi.argv(0));
		strcat (text, " ");
		strcat (text, gi.args());
	}
	else
	{
		p = gi.args();

		if (*p == '"')
		{
			p++;
			p[strlen(p)-1] = 0;
		}
		strcat(text, p);
	}

	// don't let text be too long for malicious reasons
	if (strlen(text) > 150)
		text[150] = 0;

	strcat(text, "\n");

	if (flood_msgs->value) {
		cl = ent->client;

        if (level.time < cl->flood_locktill) {
			gi.cprintf(ent, PRINT_HIGH, "You can't talk for %d more seconds\n",
				(int)(cl->flood_locktill - level.time));
            return;
        }
        i = cl->flood_whenhead - flood_msgs->value + 1;
        if (i < 0)
            i = (sizeof(cl->flood_when)/sizeof(cl->flood_when[0])) + i;
		if (cl->flood_when[i] && 
			level.time - cl->flood_when[i] < flood_persecond->value) {
			cl->flood_locktill = level.time + flood_waitdelay->value;
			gi.cprintf(ent, PRINT_CHAT, "Flood protection:  You can't talk for %d seconds.\n",
				(int)flood_waitdelay->value);
            return;
        }
		cl->flood_whenhead = (cl->flood_whenhead + 1) %
			(sizeof(cl->flood_when)/sizeof(cl->flood_when[0]));
		cl->flood_when[cl->flood_whenhead] = level.time;
	}

	if (dedicated->value)
		gi.cprintf(NULL, PRINT_CHAT, "%s", text);

	for (j = 1; j <= game.maxclients; j++)
	{
		other = &g_edicts[j];
		if (!other->inuse)
			continue;
		if (!other->client)
			continue;
		if (team)
		{
			if (!OnSameTeam(ent, other))
				continue;
		}
		gi.cprintf(other, PRINT_CHAT, "%s", text);
	}
}

void Cmd_PlayerList_f(edict_t *ent)
{
	int i;
	char st[80];
	char text[1400];
	edict_t *e2;

	// connect time, ping, score, name
	*text = 0;
	for (i = 0, e2 = g_edicts + 1; i < maxclients->value; i++, e2++) {
		if (!e2->inuse)
			continue;

		Com_sprintf(st, sizeof(st), "%02d:%02d %4d %3d %s%s\n",
			(level.framenum - e2->client->resp.enterframe) / 600,
			((level.framenum - e2->client->resp.enterframe) % 600)/10,
			e2->client->ping,
			e2->client->resp.score,
			e2->client->pers.netname,
			e2->client->resp.spectator ? " (spectator)" : "");
		if (strlen(text) + strlen(st) > sizeof(text) - 50) {
			sprintf(text+strlen(text), "And more...\n");
			gi.cprintf(ent, PRINT_HIGH, "%s", text);
			return;
		}
		strcat(text, st);
	}
	gi.cprintf(ent, PRINT_HIGH, "%s", text);
}


/*
=================
ClientCommand
=================
*/
void ClientCommand (edict_t *ent)
{
	char	*cmd;

	if (!ent->client)
		return;		// not fully in game yet

	cmd = gi.argv(0);

	if (Q_stricmp (cmd, "players") == 0)
	{
		Cmd_Players_f (ent);
		return;
	}
	if (Q_stricmp (cmd, "say") == 0)
	{
		Cmd_Say_f (ent, qfalse, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "say_team") == 0)
	{
		Cmd_Say_f (ent, qtrue, qfalse);
		return;
	}
	if (Q_stricmp (cmd, "score") == 0)
	{
		Cmd_Score_f (ent);
		return;
	}
	if (Q_stricmp (cmd, "help") == 0)
	{
		Cmd_Help_f (ent);
		return;
	}

	if (level.intermissiontime)
		return;

	if (Q_stricmp(cmd, "use") == 0)
		Cmd_Use_f(ent);
	else if (Q_stricmp(cmd, "drop") == 0)
		Cmd_Drop_f(ent);
	else if (Q_stricmp(cmd, "give") == 0)
		Cmd_Give_f(ent);
	else if (Q_stricmp(cmd, "god") == 0)
		Cmd_God_f(ent);
	else if (Q_stricmp(cmd, "notarget") == 0)
		Cmd_Notarget_f(ent);
	else if (Q_stricmp(cmd, "noclip") == 0)
		Cmd_Noclip_f(ent);
	else if (Q_stricmp(cmd, "inven") == 0)
		Cmd_Inven_f(ent);
	else if (Q_stricmp(cmd, "invnext") == 0)
		SelectNextItem(ent, -1);
	else if (Q_stricmp(cmd, "invprev") == 0)
		SelectPrevItem(ent, -1);
	else if (Q_stricmp(cmd, "invnextw") == 0)
		SelectNextItem(ent, IT_WEAPON);
	else if (Q_stricmp(cmd, "invprevw") == 0)
		SelectPrevItem(ent, IT_WEAPON);
	else if (Q_stricmp(cmd, "invnextp") == 0)
		SelectNextItem(ent, IT_POWERUP);
	else if (Q_stricmp(cmd, "invprevp") == 0)
		SelectPrevItem(ent, IT_POWERUP);
	else if (Q_stricmp(cmd, "invuse") == 0)
		Cmd_InvUse_f(ent);
	else if (Q_stricmp(cmd, "invdrop") == 0)
		Cmd_InvDrop_f(ent);
	else if (Q_stricmp(cmd, "weapprev") == 0)
		Cmd_WeapPrev_f(ent);
	else if (Q_stricmp(cmd, "weapnext") == 0)
		Cmd_WeapNext_f(ent);
	else if (Q_stricmp(cmd, "weaplast") == 0)
		Cmd_WeapLast_f(ent);
	else if (Q_stricmp(cmd, "kill") == 0)
		Cmd_Kill_f(ent);
	else if (Q_stricmp(cmd, "putaway") == 0)
		Cmd_PutAway_f(ent);
	else if (Q_stricmp(cmd, "wave") == 0)
		Cmd_Wave_f(ent);
	else if (Q_stricmp(cmd, "playerlist") == 0)
		Cmd_PlayerList_f(ent);
	else if (Q_stricmp(cmd, "ent_create") == 0)
		Cmd_Ent_Create_f(ent);
	else if (Q_stricmp(cmd, "ent_fire") == 0)
		Cmd_Ent_Fire_f(ent);
	else if (Q_stricmp(cmd, "ent_remove") == 0)
		Cmd_Ent_Remove_f(ent);
	else if (Q_stricmp(cmd, "ent_setname") == 0)
		Cmd_Ent_Name_f(ent);
	else if (Q_stricmp(cmd, "mdk") == 0)
		Cmd_Mdk_f(ent);
	else if (Q_stricmp(cmd, "shoot") == 0)
		Cmd_Shoot_f(ent);
	else	// anything that doesn't match a command will be a chat
		Cmd_Say_f (ent, qfalse, qtrue);
}
