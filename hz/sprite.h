// HZ Engine Source
// Copyright (C) 1998 by David W. Jeske

// SPRITE_H
//


#ifndef SPRITE_H
#define SPRITE_H

#ifdef OS_WIN
#include <ddraw.h>
#endif

#include "spritet.h"

#ifndef SHORT
#define SHORT short int
#endif

class Map;

class SpriteTmplManager {
public:	
	struct {
		int width;
		int height;
	} max_sprite;
	
};

enum sprite_type_enum
{
  OBJ_UNKNOWN = 0,
  OBJ_LUA
};


class SpriteList;


struct hitbox_struct {
	int upper_left_x;
	int upper_left_y;
	int lower_right_x;
	int lower_right_y;
	int width;
	int height;
};

struct img_frame_struct {
	int struct_size; // size of this struct in memory
	LPDIRECTDRAWSURFACE surf; // this frame's surface
	RECT src; // source rect in surface
	POINT offset; // offset of center from U/L

	int num_hitbox;
	struct hitbox_struct hitboxes[1];
	// do *NOT* add more after this.. there will be more
	// hitboxes dynamically allocated...
};


struct img_frame_dir_struct {
	int num_dir;
	struct img_frame_struct *dir;
};


struct img_frame_state_struct {
	int num_states;
	struct img_frame_dir_struct *states;
};


class Sprite  {

 private:
	// ----------- for spritelist
	friend class SpriteList; // these are for SpriteList fast access
	void linkObject(Sprite *);
	Sprite *next; // link to next node
	Sprite *prev; // link to previous node
        // --------------------------
	
	// ----------- for Map and ViewPort
	friend class Map;
	friend class ViewPort;
	Sprite *tile_next; // link to next node on this tile location
	unsigned char z_value; // 0 = stationary on map, 0xFF = TOP
	Sprite **my_map_loc; // pointer to the link from the map to this list
	int old_tile_x, old_tile_y; // "old" location in tile coordinates
	Map *myMap;
	void addObjectToMap(Sprite **map_loc); // add me to a map location	
	void removeObjectFromMap(); // removes myself from the map I'm in...
	// --------------------------------

 protected:
        int nearby_check; // check for nearby objects
	void placeObject(Map *aMap); // place an object on the map
  	int should_die;
 public:
	enum sprite_type_enum type;  // object type

	int mynumber; // my sprite number...
	char *obj_type_string;
	double velx, vely; // x and y velocity (pixels/millisecond)
	double posx, posy; // actual x and y position

	Sprite(SpriteList *aList,SpriteType *a_type, double x, double y, 
			double vx, double vy);
 protected:
	~Sprite(); // destructor
 public:
	
	virtual void SpriteTeardown(void);

	
	Sprite *checkCollision();
		SpriteType *mySpriteTypeObj; // connection to my SpriteType
	SpriteList *mySpriteList;
	int layer;
	void setLayer(int new_layer);
	void goToLoc(double newx, double newy); // called indirectly from Lua thru C_obj_goto
	void Die(void); // we should die!
	void Draw(int ul_x, int ul_y);
	void DrawClipped(int ul_x, int ul_y, RECT *clip_rect);
	void spriteDoTick(unsigned int tickDiff); // run the doTick() show


	// callbacks
	virtual void doTick(unsigned int tickDiff) = 0; // move the object
	virtual void doAITick(unsigned int tickDiff) = 0;
	virtual int canCollide(void);
	virtual int handleEvent(struct input_event *ev) = 0; // we want key events!
	virtual void handleCollision(Sprite *obj_hit) = 0; // a collision occured!
	virtual const char *getPropertyStr(const char *propName) = 0; // get object property value
        virtual void handleNearbyObjects(Sprite *nearby_objects[],int nearby_objects_count);
};


typedef Sprite DBLNODE;
typedef DBLNODE *LPDBLNODE;

class SpriteList {
private:
		Sprite *myHead;
		int myNum;
public:
		SpriteList();
		~SpriteList();
		void addSprite(Sprite *);
		void removeSprite(Sprite *);
		void doTick(unsigned int tickDiff);
		void doAITick(unsigned int tickDiff);
};

extern SpriteList *defaultSpriteList;
extern SpriteTmplManager *spriteTmplManager;

#define MAX_SPRITE_DIR 40
extern float Dirx[MAX_SPRITE_DIR];
extern float Diry[MAX_SPRITE_DIR];



#endif /* SPRITE_H */
