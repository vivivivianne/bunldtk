/* ldtk.h - header file for ldtk.c, 
 * defines various data structures as 
 * well as provides the functions to 
 * parse and use the structures */

#pragma once
#include <json-c/json.h> 
#include "bunarr.h" 

typedef enum
{
	LDTK_EXTENSION_LDTK=  0x00000100,		/**< Uses .ldtkl as the extension for the main ldtk file */ // DONE
	LDTK_EXTENSION_JSON= 0x00000200,		/**< Uses .json as the extension for the main ldtk file */ // DONE
	LDTK_LEVEL_GREEDY_MESH= 0x00004000,		/**< Enables greedy meshing on the level walls, returning rectlanges with w, and h */ // DONE
	LDTK_LEVEL_GRID_PARTITION= 0x00000800,		/**< Enables a grid spatial partitioning and assigns a grid cell value to each wall*/ //TBA
	LDTK_MULTI_WORLD_ENABLE = 0x00002000,		/**< Enables Multi World Support */ //TBA

} LDTK_FLAGS;

typedef enum
{
	LVL_KEEP_NGBR=  0x00000001,	/**< Does not destroy the lvl->neighbours array */ 
	LVL_KEEP_PATH= 0x00000002,	/**< Does not destroy the lvl->path arr */
	LVL_KEEP_TILES= 0x00000004,	/**< Does not destroy the lvl->tiles arr */
	LVL_KEEP_FIELDS= 0x00000008,	/**< Does not destroy the lvl->custom_fields json_object*/
} LDTK_LVL_FLAGS;

typedef enum {
	LDTK_LAYER_TILES,
	LDTK_LAYER_INTGRID,
	LDTK_LAYER_ENTITY,
}LDTK_LAYER_TYPE;

typedef struct ldtk_rect{
	i32 x, y, w, h;
}ldtk_rect;

typedef struct ldtk_wall{
	ldtk_rect bb;
	u8 type;
}ldtk_wall;

typedef struct ldtk_tile {
	ldtk_rect rect;
	ldtk_rect src;
	u16 t;		//tile num
	u8 f; 		//flip: 0= not flipped, 1 = flipx, 2 = flipy 3 = flipxy
}ldtk_tile;

typedef struct ldtk_layer {
	LDTK_LAYER_TYPE type;
	u32 z;
	u16 tilesize;
	char *tileset_path; // null if entity layer
	char *composite; 	// will be null unless you enalbe ldtk_PNG_LAYER or LDTK_PNG_BOTH
	bunarr *content; // change so we actually only have one type of layer
}ldtk_layer;


typedef struct ldtk_entity { // add support for multiple entity layers later
	json_object *custom_fields;
	ldtk_rect rect;
	u8 r,g,b;
}ldtk_ent;

typedef struct ldtk_level {
	json_object *custom_fields;
	ldtk_rect rect;
	u8 r,g,b;

	char *id;
	char *path;
	// char *composite; 	// will be null unless you enalbe ldtk_PNG_LEVEL or LDTK_PNG_BOTH
	char *bg_tile_path;

	bunarr *walls;		
	bunarr *layers; 	//change so we only have one layer type
	bunarr *ngbrs;
}ldtk_lvl;

typedef struct ldtk_neighbour {
	char *path;
	char dir[3];
	u32 id;
}ldtk_ngbr;

typedef struct ldtk_system{
	LDTK_FLAGS flags;
	u32 tl_size;
	char *prj_dir;
	char *prj_name;
}ldtk_sys;

/** \brief Initilze the ldtk loading system,it will read you json 
 * briefly to check your settings, you can also enable extra functionalities 
 * \param tl_size tile size
 * \param prj_name a string with the name of your ldtk project 
 * \param prj_dir the directory where your ldtk project is
 * \param flags one or more ldtk_FLAGS or'ed together */
void ldtk_init(u32 tl_size, char *prj_name, char *lvl_dir, LDTK_FLAGS flags);

/** \brief Load level from level name
 * \param lname the name of the level to be loaded
 * \return *ldtk_lvl a pointer to the populated level struct */
ldtk_lvl *ldtk_load_lvl(char *path);

/** \brief find path of level with idd
 * \param iid a String with the level
 * \param dst the string where the level name will be saved at */
void ldtk_get_lvl_name(char *iid, char *path);

/** \brief Destroys a ldtk level structure */
void ldtk_destroy_lvl(ldtk_lvl *lvl);

/** \brief Destroys the level, but with more options*/
void ldtk_destroy_lvl_ex(ldtk_lvl *lvl, LDTK_LVL_FLAGS flags);

/** \brief Returns a pointer to a malloc'ed value of the requested level custom field, you must free the pointer after using it*/
void *ldtk_get_lvl_field(ldtk_lvl *lvl,char* field);

/** \brief Returns a pointer to a malloc'ed value of the requested Entity custom field, you must free the pointer after using it*/
void *ldtk_get_ent_field(ldtk_ent *ent,char* field);

/** \brief gest a malloced pointer with the contents of the desired field, 
 * please free the pointer after using it */
void *ldtk_get_field(json_object *custom_fields,char* field);
